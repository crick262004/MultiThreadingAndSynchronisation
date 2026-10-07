// JOB DEPENDENCY SCHEDULER
// Given black-box: get_next_jobs(finished) -> ready jobs
// Design: scheduler loop sleeps on CV, wakes when any job finishes,
// re-queries get_next_jobs, dispatches new ready jobs to thread pool.

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <chrono>

// -- Assume ThreadPool exists (see ThreadPool.cpp) --
class ThreadPool { /* same as ThreadPool.cpp, included here to compile */
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            { std::unique_lock<std::mutex> lock(mtx);
              cv.wait(lock, [this]{ return stop || !tasks.empty(); });
              if (stop && tasks.empty()) return;
              task = std::move(tasks.front()); tasks.pop(); }
            task();
        }
    }
public:
    explicit ThreadPool(size_t n) {
        for (size_t i = 0; i < n; ++i) workers.emplace_back(&ThreadPool::workerLoop, this);
    }
    void enqueue(std::function<void()> f) {
        { std::lock_guard<std::mutex> lg(mtx); tasks.push(std::move(f)); }
        cv.notify_one();
    }
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lg(mtx); stop = true; }
        cv.notify_all();
        for (auto& w : workers) if (w.joinable()) w.join();
    }
};

// -- Assume get_next_jobs & job registry are given --
struct JobRegistry {
    std::unordered_map<std::string, std::vector<std::string>> deps;
    std::unordered_map<std::string, std::function<void()>> funcs;
    void addJob(const std::string& name, std::vector<std::string> parents, std::function<void()> work) {
        deps[name] = std::move(parents);
        funcs[name] = std::move(work);
    }
    std::vector<std::string> get_next_jobs(const std::unordered_set<std::string>& finished) {
        std::vector<std::string> ready;
        for (auto& [job, parents] : deps) {
            if (finished.count(job)) continue;
            bool ok = true;
            for (auto& p : parents) if (!finished.count(p)) { ok = false; break; }
            if (ok) ready.push_back(job);
        }
        return ready;
    }
};

// ===================== THE CORE DESIGN =====================

class JobScheduler {
    ThreadPool& pool;
    JobRegistry& registry;
    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_set<std::string> finished;
    std::unordered_set<std::string> dispatched;
    int inFlight = 0;

public:
    JobScheduler(ThreadPool& p, JobRegistry& r) : pool(p), registry(r) {}

    void run() {
        std::unique_lock<std::mutex> lock(mtx);

        while (true) {
            auto ready = registry.get_next_jobs(finished);

            for (auto& job : ready) {
                if (dispatched.count(job)) continue;   // already running
                dispatched.insert(job);
                inFlight++;

                pool.enqueue([this, job] {
                    registry.funcs[job]();              // do the work

                    std::lock_guard<std::mutex> lg(mtx);
                    finished.insert(job);
                    inFlight--;
                    cv.notify_one();                    // wake scheduler
                });
            }

            if (inFlight == 0) break;                  // all done

            // sleep until a job finishes
            size_t prev = finished.size();
            cv.wait(lock, [&]{ return finished.size() > prev; });
        }
    }
};


int main() {
    ThreadPool pool(4);
    JobRegistry reg;
    auto work = [](int ms){ return [ms]{ std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }; };

    reg.addJob("A", {},         work(200));
    reg.addJob("B", {},         work(100));
    reg.addJob("C", {"A"},      work(150));
    reg.addJob("D", {"A"},      work(100));
    reg.addJob("E", {"B"},      work(200));
    reg.addJob("F", {"C","D"},  work(100));
    reg.addJob("G", {"F","E"},  work(50));

    JobScheduler scheduler(pool, reg);
    std::cout << "Running...\n";
    scheduler.run();
    std::cout << "All done.\n";
}
