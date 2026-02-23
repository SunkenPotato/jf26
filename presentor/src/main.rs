use std::env::var;
#[cfg(feature = "mixer")]
use std::fs::File;

#[cfg(feature = "mixer")]
use anyhow::Context;
use axum::{Json, extract::Query, routing::get};
use log::{error, info, warn};
#[cfg(feature = "mixer")]
use reqwest::Client;
use reqwest::StatusCode;
use serde::{Deserialize, Serialize};
#[cfg(feature = "mixer")]
use tokio::spawn;
use tokio::{fs::rename, net::TcpListener};

#[cfg(all(feature = "mixer", not(target_os = "linux")))]
const _: () = compile_error!(
    "The `server` feature may only be enabled on Linux, due system calls being made that may not exist on other platforms"
);

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let now = chrono::Utc::now();
    let bind_addr = var("SERVER_ADDR")?;
    let target_serv = var("TARGET_ADDR")?;

    log4rs::init_file("log4rs.yaml", Default::default())?;

    #[cfg(not(target_os = "linux"))]
    warn!(
        "Detected platform is not Linux, disabling TRNG sampling. No entropy will be added to /dev/random."
    );

    info!("Hello, world!");
    info!("SERVER_ADDR={bind_addr};TARGET_ADDR={target_serv}");

    let listener = TcpListener::bind(bind_addr).await?;

    info!("Bound listener to specified address");

    #[cfg(feature = "mixer")]
    let _fetcher_task = spawn(fetcher_task(target_serv)?);

    let router = axum::Router::new()
        .route("/mixer", get(svr))
        .fallback_service(tower_http::services::ServeDir::new("static"));

    info!("Starting server");

    tokio::select! {
        s = axum::serve(listener, router) => s?,
        _ = tokio::signal::ctrl_c() => {
            let formatted = now.format("%H-%M-%s_%Y-%m-%d");
            rename("log/latest.log", format!("log/{formatted}.log")).await?;
        }
    };

    Ok(())
}

#[derive(Deserialize)]
struct ServerQuery {
    start: u8,
    stop: u8,
}

#[derive(Serialize)]
struct MixerResponse {
    code: u8,
}

#[axum::debug_handler]
async fn svr(Query(query): Query<ServerQuery>) -> Result<Json<MixerResponse>, StatusCode> {
    let (start, stop) = (query.start, query.stop);

    let mut byte = [0u8; 1];
    if let Err(e) = getrandom::fill(&mut byte) {
        error!("Failed to request random bytes from system: {e:#?}");
        return Err(StatusCode::SERVICE_UNAVAILABLE);
    }

    let code = ((byte[0] as f32 / 255f32) * stop as f32 + start as f32) as u8;

    Ok(Json(MixerResponse { code }))
}

#[cfg(feature = "mixer")]
fn fetcher_task(base: String) -> anyhow::Result<impl Future<Output = ()>> {
    use std::fs::OpenOptions;

    use reqwest::{Client, Method};
    use tokio::{runtime::Handle, task::block_in_place};

    let entropy_pool = OpenOptions::new()
        .write(true)
        .open("/dev/random")
        .with_context(
            || "Server needs to be run with superuser or privileges to write to /dev/random",
        )?;
    let client = Client::new();
    // test connection
    block_in_place(|| Handle::current().block_on(client.request(Method::OPTIONS, &base).send()))?;

    Ok(fetcher_task_inner(base, client, entropy_pool))
}

#[cfg(feature = "mixer")]
async fn fetcher_task_inner(base: String, client: Client, pool: File) {
    use libc::c_int;

    #[repr(C)]
    struct EntropyPoolPayload {
        entropy_count: c_int,
        buf_size: c_int,
        buf: [u8; 512],
    }

    loop {
        use std::time::Duration;

        use tokio::time::sleep;

        let response = match client.get(format!("{base}/?amount=512")).send().await {
            Ok(v) => v,
            Err(e) => {
                log::error!("Failed to retrieve bytes from server, going into panic mode: {e:#?}");
                return;
            }
        };

        if response.status() == StatusCode::SERVICE_UNAVAILABLE {
            info!("Server unable to provide bytes, retrying in three minutes");
            sleep(Duration::new(60 * 3, 0)).await;
            continue;
        }

        if !response.status().is_success() {
            error!("Server returned an unexpected response: {response:#?}, exiting.");
            return;
        }

        let bytes = match response.bytes().await {
            Ok(v) => v,
            Err(e) => {
                error!("Failed to extract bytes, going into panic mode: {e:#?}");
                return;
            }
        };

        let Ok(slice) = (&bytes as &[u8]).try_into() else {
            use std::time::Duration;

            use tokio::time::sleep;

            sleep(Duration::from_mins(3)).await;
            continue;
        };

        let payload = EntropyPoolPayload {
            entropy_count: (512 * 8),
            buf_size: 512,
            buf: slice,
        };

        unsafe {
            use std::os::fd::AsRawFd;

            use libc::ioctl;

            if ioctl(pool.as_raw_fd(), 1074287107, &payload) != 0 {
                error!("Failed to inject custom entropy into system entropy pool");
            } else {
                info!("Successfully injected custom entropy into system entropy pool (512B)");
            }
        }

        sleep(Duration::new(60 * 3, 0)).await;
    }
}
