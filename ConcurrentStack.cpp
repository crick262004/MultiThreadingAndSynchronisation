/*
could use a std::counting_semaphore to track the number of items in the stack. Producers release() after pushing (incrementing the count), and consumers acquire() before popping (blocking until count > 0). You'd still need a mutex to protect the actual stack data structure, but the semaphore replaces the condition variable for the "wait until something is available" part.
*/
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <optional>
#include <stack>
#include <string>

template <typename T>
class ConcurrentStack {
    std::stack<T> data;
    mutable std::mutex mtx;
    std::condition_variable cv;

public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            data.push(std::move(value));
        }
        cv.notify_one(); // notify after releasing
    }

    // Blocking pop — waits until an element is available.
    T wait_and_pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !data.empty(); });
        T value = std::move(data.top());
        data.pop();
        return value;
    }

    // Non-blocking pop — returns std::nullopt if empty. 
    // "non-blocking" here means it won't suspend the caller waiting for a producer to push something. 
    // It is still blocking the thread.

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (data.empty()) return std::nullopt;
        T value = std::move(data.top());
        data.pop();
        return value;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return data.size();
    }
};

int main() {
    ConcurrentStack<int> stack;
    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 3;
    const int ITEMS_PER_PRODUCER = 5;

    std::vector<std::jthread> producers;
    std::vector<std::jthread> consumers;

    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back([&stack, i] {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                int val = i * 100 + j;
                stack.push(val);
                std::string msg = "Producer " + std::to_string(i) + " pushed " + std::to_string(val) + "\n";
                std::cout << msg;
            }
        });
    }

    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([&stack, i] {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                int val = stack.wait_and_pop();
                std::string msg = "Consumer " + std::to_string(i) + " popped " + std::to_string(val) + "\n";
                std::cout << msg;
            }
        });
    }

    // jthreads join automatically on destruction
    return 0;
}
