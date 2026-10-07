#include <iostream>
#include <thread>
#include <shared_mutex>
#include <vector>
#include <chrono>
#include <string>

// std::shared_mutex allows two lock modes:
//   shared_lock   — multiple readers can hold it simultaneously
//   unique_lock   — one writer, blocks everyone else
//
// This is the same reader/writer pattern as Readers-Writers.cpp,
// but using the standard library instead of hand-rolled semaphores/CVs.

class Config {
private:
    std::string value;
    std::shared_mutex mtx;

public:
    Config(const std::string& initial) : value(initial) {}

    std::string read(int readerId) {
        std::shared_lock<std::shared_mutex> lock(mtx); // multiple readers can enter
        std::cout << "Reader " << readerId << " reads: " << value << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return value;
    }

    void write(const std::string& newValue) {
        std::unique_lock<std::shared_mutex> lock(mtx); // exclusive access
        std::cout << "Writer updating: " << value << " -> " << newValue << std::endl;
        value = newValue;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
};

int main() {
    Config config("v1.0");

    std::vector<std::thread> threads;

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&config, i] {
            config.read(i);
        });
    }

    threads.emplace_back([&config] {
        config.write("v2.0");
    });

    for (int i = 3; i < 6; ++i) {
        threads.emplace_back([&config, i] {
            config.read(i);
        });
    }

    for (auto& t : threads)
        t.join();

    return 0;
}
