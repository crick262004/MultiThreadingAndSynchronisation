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
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;

    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx);
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

          Broken down:
        
          - workers — a std::vector<std::thread>
          - emplace_back(...) — constructs a std::thread in place at the back of the vector
          - &ThreadPool::workerLoop — pointer to the member function. The &ClassName::method
            syntax is how you take the address of a member function in C++
          - this — the object instance to call it on
        
          When you pass a member function to std::thread, you need two things: the function
          pointer and the object it should run on. So this is essentially saying "start a thread
          that calls this->workerLoop()".
        
          It's equivalent to the lambda version I had earlier:
        
          workers.emplace_back([this] { workerLoop(); });
        
          Both do the same thing — spawn a thread running workerLoop on the current ThreadPool
          object. The member-function-pointer form is just the direct way; the lambda is the
          wrapper way.
        */
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            tasks.push(std::move(task));
        }
        cv.notify_one();   // wake one sleeping worker (no-op if all busy)
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();   // wake everyone so they see the flag and exit
        for (auto& w : workers)
            if (w.joinable()) w.join();
    }

};

// -------------------------------------------------------------------------
// Demo
// -------------------------------------------------------------------------
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
    return 0;

}
