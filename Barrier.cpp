#include <iostream>
#include <thread>
#include <barrier>
#include <vector>
#include <string>

// std::barrier is a reusable synchronization point.
// All N threads must arrive before any can proceed — then it resets
// and can be used again. Like a latch, but multi-use.

const int NUM_THREADS = 3;
const int NUM_ROUNDS = 3;

std::barrier sync_point(NUM_THREADS);

void worker(int id) {
    for (int round = 1; round <= NUM_ROUNDS; ++round) {
        std::string msg = "Thread " + std::to_string(id) + " finished round " + std::to_string(round) + "\n";
        std::cout << msg;

        sync_point.arrive_and_wait(); // block until all threads reach here
    }
}

int main() {
    std::vector<std::jthread> threads;
    for (int i = 0; i < NUM_THREADS; ++i)
        threads.emplace_back(worker, i);

    // No need for explicit joining; std::jthread automatically joins when threads vector goes out of scope.


    return 0;
}
