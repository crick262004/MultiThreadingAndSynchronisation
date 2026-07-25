#include <iostream>
#include <thread>
#include <queue>
#include <semaphore>
#include <mutex>
#include <chrono>

const int MAX_BUFFER_SIZE = 5;
std::queue<int> buffer;

// Semaphores for signaling state
std::counting_semaphore<MAX_BUFFER_SIZE> emptySlots(MAX_BUFFER_SIZE);
std::counting_semaphore<MAX_BUFFER_SIZE> fullSlots(0);

// Mutex for locking the critical section
std::mutex bufferMutex;

void producer(int id) {
    for (int i = 1; i <= 5; ++i) {
        // 1. Wait for an empty slot to become available (Signaling)
        emptySlots.acquire();
        
        // 2. Lock the buffer to safely add data (Locking)
        bufferMutex.lock();
        buffer.push(i);
        std::cout << "[Producer " << id << "] Added: " << i 
                  << " | Buffer Size: " << buffer.size() << "\n";
        bufferMutex.unlock();
        
        // 3. Signal to consumers that a new item is available
        fullSlots.release();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer(int id) {
    for (int i = 1; i <= 5; ++i) {
        // 1. Wait for an item to be produced (Signaling)
        fullSlots.acquire();
        
        // 2. Lock the buffer to safely remove data (Locking)
        bufferMutex.lock();
        int item = buffer.front();
        buffer.pop();
        std::cout << "  [Consumer " << id << "] Removed: " << item 
                  << " | Buffer Size: " << buffer.size() << "\n";
        bufferMutex.unlock();
        
        // 3. Signal to producers that a slot has freed up
        emptySlots.release();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

int main() {
    // Spin up multiple producer and consumer threads
    std::thread p1(producer, 1);
    std::thread p2(producer, 2);
    std::thread c1(consumer, 1);
    std::thread c2(consumer, 2);

    // Wait for all threads to finish execution
    p1.join();
    p2.join();
    c1.join();
    c2.join();

    return 0;
}
