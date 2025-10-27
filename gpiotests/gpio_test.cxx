#include <pigpio.h>
#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>

std::atomic<int> counter{0};
bool running = true;

void count_callback(int gpio, int level, uint32_t tick)
{
    if (level == 0) // Falling edge
    {
        int c = ++counter;
        std::cout << c << std::endl;
    }
}

void signal_handler(int signum)
{
    running = false;
}

int main()
{
    std::cout << "init GPIO" << std::endl;
    if (gpioInitialise() < 0)
    {
        std::cerr << "pigpio initialization failed" << std::endl;
        return 1;
    }

    std::cout << "init SH" << std::endl;
    // Handle Ctrl+C clean exit
    std::signal(SIGINT, signal_handler);

    int pin = 17; // BCM pin number
    std::cout << "set mode" << std::endl;

    if (gpioSetMode(pin, PI_INPUT) != 0) {
        std::cerr << "could not set input mode for bcm 17" << std::endl;
        return 1;
    }

    std::cout << "reg clbk" << std::endl;
    // Register callback on falling edge
    if (gpioSetAlertFunc(pin, count_callback) != 0) {
        std::cerr << "failed to set alert fn" << std::endl;
        return 1;
    };

    // Keep the program alive
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    gpioTerminate();
    return 0;
}
