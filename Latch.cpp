#include <iostream>
#include <thread>
#include <latch>
#include <vector>
#include <string>
#include <chrono>

// std::latch is a single-use countdown barrier.
// Threads call count_down() to decrement it. Any thread that calls
// wait() blocks until the counter hits zero. Once it reaches zero,
// all waiters are released and the latch stays open forever.
//
// Use case: "don't proceed until N things have happened."

void simulate() {
    const int NUM_SERVICES = 4;
    std::latch ready(NUM_SERVICES);

    std::vector<std::string> names = {"Database", "Cache", "Auth", "Queue"};
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_SERVICES; ++i) {
        threads.emplace_back([&ready, &names, i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i + 1)));
            std::string msg = names[i] + " ready\n";
            std::cout << msg;
            ready.count_down();
        });
    }

    ready.wait(); // blocks until all 4 services have counted down
    std::cout << "All services ready — accepting requests\n";

    for (auto& t : threads)
        t.join();
}

int main() {
    simulate();
    return 0;
}
