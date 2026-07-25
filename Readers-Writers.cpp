// Reader Preference Model
/*
  Because the cv.wait() inside the writer temporarily releases the isWrite mutex while the writer sleeps, 
  new incoming readers can easily grab isWrite.lock(), increment the counter, 
  and jump in to read. As long as read_count stays above 0, 
  the writer will stay asleep, giving readers the priority.
*/

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Shared Synchronization Variables
std::mutex isWrite;
std::condition_variable cv;
std::atomic<int> read_count{0};

void writer() {
    // 1. Lock the gatekeeper mutex. No new readers/writers can enter.
    std::unique_lock<std::mutex> lock(isWrite);
    
    // 2. If readers are currently reading, sleep and wait for them to finish.
    // (cv.wait automatically unlocks 'isWrite' while sleeping, and relocks when awake)
    cv.wait(lock, []{ return read_count == 0; });
    
    // 3. --- WRITE DATA HERE ---
    // (Writer has exclusive access; read_count is 0, and 'isWrite' is locked)
    
    // 4. Mutex is automatically unlocked when 'lock' goes out of scope.
}

void reader() {
    // 1. Quickly grab the mutex to register ourselves
    isWrite.lock();
    read_count++;
    isWrite.unlock(); // Drop it immediately so other readers can join!
    
    // 2. --- READ DATA CONCURRENTLY HERE ---
    
    // 3. Unregister when done
    read_count--;
    
    // 4. If this is the absolute last reader leaving, wake up the writer
    if (read_count == 0) {
        cv.notify_one();
    }
}
