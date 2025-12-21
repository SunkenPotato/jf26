use std::{fs::OpenOptions, os::fd::AsRawFd};

use libc::{c_int, ioctl};

#[repr(C)]
struct RandPoolInfo {
    entropy_count: c_int,
    buf_size: c_int,
    buf: [u8; 32]
}

fn main() {
    let data = reqwest::blocking::get("http://127.0.0.1:8000/?amount=32").unwrap().bytes().unwrap()[..32].try_into().unwrap();

    let file = OpenOptions::new().write(true).open("/dev/random").expect("program should be executed with superuser privileges");

    let info = RandPoolInfo {
        entropy_count: (32 * 8) as c_int,
        buf_size: 32 as c_int,
        buf: data,
    };

    unsafe {
        if ioctl(file.as_raw_fd(), 1074287107, &info) != 0 {
            eprintln!("ioctl failed | may require superuser");
            return;
        }
    }

    println!("added entropy to /dev/random")
}
