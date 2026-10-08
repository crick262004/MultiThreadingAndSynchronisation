#include <iostream>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <atomic>

using namespace std;
using namespace std::chrono;

class ScheduledExecutorService {
    enum class TaskType { ONCE, FIXED_RATE, FIXED_DELAY };

    struct Task {
        function<void()> command;
        steady_clock::time_point execTime;
        milliseconds period{0};
        TaskType type = TaskType::ONCE;

        bool operator>(const Task& other) const {
            return execTime > other.execTime;
        }
    };

    // --- Scheduler state (min-heap by execution time) ---
    priority_queue<Task, vector<Task>, greater<Task>> pq_;
    mutex mtx_;
    condition_variable schedulerCv_;

    // --- Thread pool state ---
    vector<thread> workers_;
    queue<function<void()>> workQueue_;
    mutex workMtx_;
    condition_variable workCv_;

    thread schedulerThread_;
    atomic<bool> stopped_{false};

    void addTask(Task task) {
        if (stopped_) return;
        lock_guard<mutex> lock(mtx_);
        pq_.push(move(task));
        schedulerCv_.notify_one();
    }

    void submitToPool(function<void()> fn) {
        lock_guard<mutex> lock(workMtx_);
        workQueue_.push(move(fn));
        workCv_.notify_one();
    }

    void workerLoop() {
        while (true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(workMtx_);
                workCv_.wait(lock, [this] {
                    return stopped_.load() || !workQueue_.empty();
                });
                if (stopped_ && workQueue_.empty()) return;
                task = move(workQueue_.front());
                workQueue_.pop();
            }
            task();
        }
    }

    // Single scheduler thread: sleeps until the earliest task is due,
    // wakes early if a new (possibly earlier) task is added.
    void schedulerLoop() {
        while (true) {
            unique_lock<mutex> lock(mtx_);
            schedulerCv_.wait(lock, [this] {
                return stopped_.load() || !pq_.empty();
            });
            if (stopped_) return;

            auto execTime = pq_.top().execTime;
            if (execTime > steady_clock::now()) {
                schedulerCv_.wait_until(lock, execTime);
                continue;
            }

            Task task = pq_.top();
            pq_.pop();

            if (task.type == TaskType::FIXED_RATE) {
                Task next = task;
                next.execTime += task.period;
                pq_.push(move(next));
            }

            lock.unlock();

            if (task.type == TaskType::FIXED_DELAY) {
                auto cmd = task.command;
                auto period = task.period;
                submitToPool([this, cmd, period] {
                    cmd();
                    Task next;
                    next.command = cmd;
                    next.execTime = steady_clock::now() + period;
                    next.period = period;
                    next.type = TaskType::FIXED_DELAY;
                    addTask(move(next));
                });
            } else {
                submitToPool(move(task.command));
            }
        }
    }

public:
    ScheduledExecutorService(int numThreads = 4) {
        for (int i = 0; i < numThreads; i++)
            workers_.emplace_back(&ScheduledExecutorService::workerLoop, this);
        schedulerThread_ = thread(&ScheduledExecutorService::schedulerLoop, this);
    }

    ~ScheduledExecutorService() { shutdown(); }

    // Run command once after delay
    void schedule(function<void()> command, milliseconds delay) {
        Task t;
        t.command = move(command);
        t.execTime = steady_clock::now() + delay;
        t.type = TaskType::ONCE;
        addTask(move(t));
    }

    // Run command periodically — interval measured start-to-start
    void scheduleAtFixedRate(function<void()> command,
                             milliseconds initialDelay, milliseconds period) {
        Task t;
        t.command = move(command);
        t.execTime = steady_clock::now() + initialDelay;
        t.period = period;
        t.type = TaskType::FIXED_RATE;
        addTask(move(t));
    }

    // Run command periodically — gap measured end-to-start
    void scheduleWithFixedDelay(function<void()> command,
                                milliseconds initialDelay, milliseconds delay) {
        Task t;
        t.command = move(command);
        t.execTime = steady_clock::now() + initialDelay;
        t.period = delay;
        t.type = TaskType::FIXED_DELAY;
        addTask(move(t));
    }

    void shutdown() {
        bool expected = false;
        if (!stopped_.compare_exchange_strong(expected, true)) return;
        schedulerCv_.notify_all();
        workCv_.notify_all();
        if (schedulerThread_.joinable()) schedulerThread_.join();
        for (auto& w : workers_)
            if (w.joinable()) w.join();
    }
};

// ───────────────── Demo ─────────────────

int main() {
    auto start = steady_clock::now();
    auto elapsed = [&start] {
        return duration_cast<milliseconds>(steady_clock::now() - start).count()
               / 1000.0;
    };
    mutex coutMtx;

    ScheduledExecutorService executor(3);

    // 1) One-shot after 1s
    executor.schedule([&] {
        lock_guard<mutex> lg(coutMtx);
        cout << "[" << elapsed() << "s] ONE-SHOT executed\n";
    }, 1000ms);

    // 2) Fixed-rate: first at 2s, then every 3s (start-to-start)
    executor.scheduleAtFixedRate([&] {
        lock_guard<mutex> lg(coutMtx);
        cout << "[" << elapsed() << "s] FIXED-RATE tick\n";
    }, 2000ms, 3000ms);

    // 3) Fixed-delay: first at 2s, task takes 1s, then 3s gap after finish
    //    expect runs at: 2s, 6s, 10s, 14s ...
    executor.scheduleWithFixedDelay([&] {
        {
            lock_guard<mutex> lg(coutMtx);
            cout << "[" << elapsed() << "s] FIXED-DELAY start\n";
        }
        this_thread::sleep_for(1000ms);
        {
            lock_guard<mutex> lg(coutMtx);
            cout << "[" << elapsed() << "s] FIXED-DELAY end\n";
        }
    }, 2000ms, 3000ms);

    this_thread::sleep_for(16s);
    executor.shutdown();
    cout << "Executor shut down.\n";
}
