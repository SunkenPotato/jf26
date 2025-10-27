#include <csignal>
#include <cstdio>
#include <iostream>
#include <thread>
#include <atomic>
#include "../include/libtdtp.h"

#define MPSC_CHANNEL_BUF_SIZE 8192

std::atomic_bool running{true};
void *rx;

void pkt_handler(bool *result) {
    IncomingDataPacket packet;
    // check if we've received any Ctrl-C signals yet
    while (running.load()) {
        int res = c_client_channel_try_recv(&packet, rx);
        // the other side of the channel has hung up, which should not happen.
        if (res == 1) {
            *result = false;
            return;
        }
        // no packets, continue
        else if (res == 2) {
            continue;
        }

        std::cout << "Got packet: " << packet << std::endl;
    }

    *result = true;
    return;
}

void signal_handler(int sig) {
    // let the packet handler thread know we've received a Ctrl-C signal.
    running.store(false);
    // drop the receiver, so that the client exits and frees up the main thread.
    c_free_client_receiver(rx);
}

int main() {
    // setup the Ctrl-C signal handler
    signal(SIGINT, signal_handler);

    // create the channel pair for TX/RX
    ChannelPair pair = c_client_channel(MPSC_CHANNEL_BUF_SIZE);

    // this will indicate the result of the thread
    bool thread_res;
    rx = pair.rx;
    std::thread handler(pkt_handler, &thread_res);

    // connect to tdtp://127.0.0.1:25565
    int client_res = c_data(127, 0, 0, 1, 25565, pair.tx);

    // join the thread, this is obviously after Ctrl-C has occurred and the client has exited because the channel was dropped.
    if (handler.joinable()) handler.join();

    if (client_res != 0) {
        perror("c_data");
        return 1;
    }

    if (!thread_res) {
        std::cerr << "Receiver was unexpectedly dropped" << std::endl;
        return 1;
    }

    return client_res;
}
