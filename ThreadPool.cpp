// =========================================================================
// THREAD POOL
// N long-lived worker threads pull tasks from a shared queue.
// Workers run an infinite loop: pop a task, run it, repeat.
// They only sleep (on a condition variable) when the queue is empty,
// so no CPU is burned polling. enqueue() wakes one sleeping worker.
//
// Note: cv.notify_one() does NOT guarantee FIFO — the OS picks which
// sleeping thread wakes. This is fine for a pool: all workers are
// identical, so it doesn't matter which one picks up the task.
// =========================================================================

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <chrono>

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks; // storing any callable object
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;

    void workerLoop() { 
        while (true) {
            // main per-thread execution: 

            std::function<void()> task; // slot to store the task we will try to pop from the tasks queue

            { // separate scope to automically handle lifetime of mutex
                std::unique_lock<std::mutex> lock(mtx); // RAII

                // Sleep only when queue is empty AND not shutting down.
                cv.wait(lock, [this] { return stop || !tasks.empty(); });

                if (stop && tasks.empty())
                    return;                       // pool shutting down, exit
                task = std::move(tasks.front());
                tasks.pop();
            }                                       // unlock before running
            task();                                 // run task outside lock
        }
    }

public:
    explicit ThreadPool(size_t n) {
    for (size_t i = 0; i < n; ++i)
        workers.emplace_back(&ThreadPool::workerLoop, this);
        /*
          It constructs a std::thread that runs workerLoop on this specific ThreadPool instance.
          - &ThreadPool::workerLoop — pointer to the member function. 
          - this — the object instance to call it on
        
          It's equivalent to the lambda version:
          workers.emplace_back([this] { workerLoop(); });
        */
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.push(std::move(task));
        }
        cv.notify_one();   // wake one sleeping worker (no-op if all busy)
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx); // use the same mutex for the stop variable as for the tasks queue
            // because of the cv guard
            // One mutex for all shared state the condition depends on: is the standard pattern.
            stop = true;
        }
        cv.notify_all();   // wake everyone so they see the flag and exit
        for (auto& w : workers)
            if (w.joinable()) w.join();
    }

};

int main() {
    ThreadPool pool(4);

    for (int i = 1; i <= 8; ++i) {
        pool.enqueue([i] {
            std::cout << "  [Task " << i << "] running on thread "
                      << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            std::cout << "  [Task " << i << "] done" << std::endl;
        });
    }

    // Destructor joins all workers (waits for queued tasks to finish).
    // main thread runs the destructor?
    return 0;

}
