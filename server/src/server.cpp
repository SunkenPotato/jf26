#include "I2B.h"

#define GPIO_PIN 17

// volatile sig_atomic_t keep_running = 1;

std::chrono::time_point<std::chrono::high_resolution_clock> program_start;

std::mutex buffer_lock;
std::vector<unsigned int> bin_buffer;

void fill_buffer(unsigned char *buf, int n)
{
    // we do not lock this because the caller is expected to lock to avoid deadlocks.
    for (int i = 0; i < n; i += 1)
    {
        unsigned char c = bin_buffer.back();
        bin_buffer.pop_back();
        // if there is another bin (quantile) number in the buffer, we amalgamate both so that we don't have half empty bytes
        if (bin_buffer.size() > 0) {
            // shift four to the right (since we've specified 2^4 quantiles, the quantile number will be in the last four bits)
            c <<= 4;
            // the last four of c will be empty and the first four of bin_buffer.back() will be empty, so we just use OR.
            c |= bin_buffer.back();
            bin_buffer.pop_back();
        }

        buf[i] = c;
    }
}

std::mutex converter_mutex;
// TODO(SunkenPotato): set to 100 for release
// 16 (2^4) quantiles will give us 4 bits per byte, which means that every byte will look like: 0000XXXX where X is a random bit.
// to get actual, full bytes, we'd have to set the #quantiles to 2^8, which will require (2^8)^2 = 35536 comparison intervals (takes forever in the average environment, since 2^4 quantiles already takes forever)
Intervall2Bin converter = Intervall2Bin(10, 16);

void initRoutes(httplib::Server &svr)
{
    svr.Get("/", [](const httplib::Request &req, httplib::Response &res)
    {
        size_t n = 32;

        if (req.has_param("amount")) {
            try {
                n = std::stoi(req.get_param_value("amount"));
                if (n <= 0 || n >= 4096) n = 32;
            } catch (...) {
                n = 32;
            }
        } else {
            res.status = 400;
            res.set_content("Missing parameter 'amount'", "text/plain");
            return;
        }

        unsigned char buffer[4096];
        std::unique_lock lock(buffer_lock);
        if (bin_buffer.size() < n) {
            res.status = 503;
            res.set_content("That amount of bytes is not available\n", "text/plain");
            return;
        }

        fill_buffer(buffer, n);
        // we can now unlock since we do not use the buffer
        lock.unlock();

        std::string_view body(reinterpret_cast<const char*>(buffer), n);
        res.set_content_provider(n, "application/octet-stream", [buffer](size_t offset, size_t len, httplib::DataSink &sink) {
            sink.write(reinterpret_cast<const char*>(buffer + offset), len);
            return true;
        });
        res.status = 200;

        return;
    });
}

std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> last_particle;

void gpioHook(int gpio, int level, unsigned int tick)
{
    if (level == 0)
    {
        if (!last_particle)
        {
            last_particle = std::chrono::high_resolution_clock::now();
            return;
        }
        else
        {
            std::chrono::time_point now = std::chrono::high_resolution_clock::now();
            unsigned int interval = std::chrono::duration_cast<std::chrono::microseconds>(now - *last_particle).count();
            last_particle = now;
            std::lock_guard guard(converter_mutex);
            std::vector bins = converter.take_intervall(interval);
            if (bins.size() == 0) {
                return;
            }

            std::lock_guard bin_buf_guard(buffer_lock);
            bin_buffer.insert(bin_buffer.end(), std::make_move_iterator(bins.begin()), std::make_move_iterator(bins.end()));
        }
    }
}

int initGpio()
{
    if (gpioInitialise() < 0)
    {
        std::cerr << "Failed to call gpioInitialise" << std::endl;
        return 1;
    };
    if (gpioSetMode(GPIO_PIN, PI_INPUT) != 0)
    {
        std::cerr << "Failed to set input mode" << std::endl;
        return 2;
    }
    if (gpioSetAlertFunc(GPIO_PIN, gpioHook) != 0)
    {
        std::cerr << "Failed to set alert function" << std::endl;
        return 3;
    }

    return 0;
}

int main()
{
    program_start = std::chrono::high_resolution_clock::now();
    if (initGpio() != 0)
    {
        std::cerr << "Failed to initialise GPIO" << std::endl;
        gpioTerminate();
        return 1;
    }
    httplib::Server svr;
    initRoutes(svr);

    svr.listen("127.0.0.1", 8000);

    gpioTerminate();
    return 0;
}
