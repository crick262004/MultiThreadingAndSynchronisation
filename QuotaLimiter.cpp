#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <queue>
#include <future>
#include <chrono>

enum class Party
{
    DEMOCRAT,
    REPUBLICAN,
    NONE
};

class Bathroom
{ // Batch + Turn Flipping
    static constexpr int MAX_OCCUPANCY = 3;

    std::mutex mu_;
    std::condition_variable cv_;

    Party current_party_ = Party::NONE;
    int occupancy_ = 0;
    int waiting_d_ = 0, waiting_r_ = 0;
    Party turn_ = Party::DEMOCRAT;

public:
    void enter(Party party, const std::string &name, int duration_sec)
    {
        // --- ENTRY ---
        {
            std::unique_lock<std::mutex> lock(mu_);

            // Register as waiting BEFORE the wait loop
            if (party == Party::DEMOCRAT)
                waiting_d_++;
            else
                waiting_r_++;

            cv_.wait(lock, [&]{
                bool room = occupancy_ < MAX_OCCUPANCY;
                bool compatible = (current_party_ == party || current_party_ == Party::NONE);
                bool other_waiting = (party == Party::DEMOCRAT) ? (waiting_r_ > 0) : (waiting_d_ > 0);
                // NOTE: intra-party ordering is non-deterministic (notify_all wakeup order is OS-dependent)
                bool my_turn = (turn_ == party) || !other_waiting || (current_party_ == party);
                // my_turn: it IS my turn, OR the other party has nobody waiting,
                //          OR my party is already inside (piggyback)
                return room && compatible && my_turn; 
            });

            // No longer waiting, now inside
            if (party == Party::DEMOCRAT)
                waiting_d_--;
            else
                waiting_r_--;

            current_party_ = party;
            occupancy_++;
        }

        // --- USE BATHROOM (no lock held) ---
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

        // --- EXIT ---
        {
            std::lock_guard<std::mutex> lock(mu_);
            occupancy_--;

            if (occupancy_ == 0)
            {
                current_party_ = Party::NONE;
                // Flip turn to the other party — starvation prevention
                turn_ = (party == Party::DEMOCRAT) ? Party::REPUBLICAN : Party::DEMOCRAT;
            }
        }
        cv_.notify_all();
    }
};

// FIFO approach: each thread gets a ticket, only the next-in-line enters.
// This will ensure no starvation, but has throughput issues
class BathroomFIFO
{
    static constexpr int MAX_OCCUPANCY = 3;

    std::mutex mu_;
    std::condition_variable cv_;

    Party current_party_ = Party::NONE;
    int occupancy_ = 0;
    int next_ticket_ = 0;    // next ticket to be served
    int ticket_counter_ = 0; // next ticket to be issued

public:
    void enter(Party party, const std::string &name, int duration_sec)
    {
        {
            std::unique_lock<std::mutex> lock(mu_);
            int my_ticket = ticket_counter_++;

            cv_.wait(lock, [&]{
                bool room = occupancy_ < MAX_OCCUPANCY;
                bool compatible = (current_party_ == party || current_party_ == Party::NONE);
                bool my_turn = (my_ticket == next_ticket_);
                return room && compatible && my_turn; 
            });

            current_party_ = party;
            occupancy_++;
            next_ticket_++;
        }

        cv_.notify_all();

        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

        {
            std::lock_guard<std::mutex> lock(mu_);
            occupancy_--;
            if (occupancy_ == 0)
                current_party_ = Party::NONE;
        }
        cv_.notify_all();
    }
};



class BathroomGated {
    static constexpr int MAX_OCCUPANCY = 3;

    struct Job {
        std::string name;
        int duration_sec;
        std::promise<void> done;
        std::chrono::steady_clock::time_point arrived;
    };

    // Min-heap comparator: shortest duration first (SJF)
    struct SJF {
        bool operator()(const std::shared_ptr<Job>& a, const std::shared_ptr<Job>& b) {
            return a->duration_sec > b->duration_sec;
        }
    };

    std::mutex mu_;
    std::condition_variable queue_cv_;  // new work arrived
    std::condition_variable drain_cv_;  // someone left the bathroom

    std::vector<std::shared_ptr<Job>> d_queue_, r_queue_;
    int occupancy_ = 0;
    bool shutdown_ = false;

    std::thread scheduler_thread_;

public:
    BathroomGated() : scheduler_thread_(&BathroomGated::schedulerLoop, this) {}

    ~BathroomGated() {
        {
            std::lock_guard<std::mutex> lock(mu_);
            shutdown_ = true;
        }
        queue_cv_.notify_one();
        scheduler_thread_.join();
    }
    std::future<void> enter(Party party, const std::string& name, int duration_sec) {
        auto job = std::make_shared<Job>();
        job->name = name;
        job->duration_sec = duration_sec;
        job->arrived = std::chrono::steady_clock::now();
        auto fut = job->done.get_future();

        {
            std::lock_guard<std::mutex> lock(mu_);
            if (party == Party::DEMOCRAT) d_queue_.push_back(job);
            else r_queue_.push_back(job);
        }
        queue_cv_.notify_one();
        return fut;
    }
private:
    void schedulerLoop() {
        std::unique_lock<std::mutex> lock(mu_);
        while (true) {
            // 1. Wait until there's work or shutdown
            queue_cv_.wait(lock, [&] {
                return !d_queue_.empty() || !r_queue_.empty() || shutdown_;
            });
            if (shutdown_ && d_queue_.empty() && r_queue_.empty()) return;

            // 2. OPEN GATE: pick party whose oldest job has waited longest
            auto oldest = [](const std::vector<std::shared_ptr<Job>>& q) {
                if (q.empty()) return std::chrono::steady_clock::time_point::max();
                return q.front()->arrived;
            };
            bool pick_d = oldest(d_queue_) <= oldest(r_queue_);
            auto& chosen = pick_d ? d_queue_ : r_queue_;

            // 3. CLOSE GATE: snapshot chosen queue into a min-heap (SJF), clear queue
            std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>, SJF> batch;
            for (auto& j : chosen) batch.push(j);
            chosen.clear();

            // 4. Process batch — backfill on every vacancy
            while (!batch.empty()) {
                while (occupancy_ < MAX_OCCUPANCY && !batch.empty()) {
                    auto job = batch.top();
                    batch.pop();
                    occupancy_++;

                    std::thread([this, job] {
                        std::this_thread::sleep_for(std::chrono::seconds(job->duration_sec));
                        job->done.set_value();

                        std::lock_guard<std::mutex> lock(mu_);
                        occupancy_--;
                        drain_cv_.notify_one();
                    }).detach();
                }

                if (!batch.empty()) {
                    drain_cv_.wait(lock, [&] { return occupancy_ < MAX_OCCUPANCY; });
                }
            }

            // 5. Wait for all occupants to finish before opening gate again
            drain_cv_.wait(lock, [&] { return occupancy_ == 0; });
        }
    }
};

int main()
{
    auto run_test = [](auto &bathroom, const std::string &label)
    {
        std::vector<std::thread> threads;

        auto spawn = [&](Party p, std::string name, int dur)
        {
            threads.emplace_back([&bathroom, p, name, dur]
                                 { bathroom.enter(p, name, dur); });
        };

        spawn(Party::DEMOCRAT, "D1", 3);
        spawn(Party::DEMOCRAT, "D2", 4);
        spawn(Party::REPUBLICAN, "R1", 5);
        spawn(Party::REPUBLICAN, "R2", 3);
        spawn(Party::DEMOCRAT, "D3", 2);
        spawn(Party::DEMOCRAT, "D4", 6);
        spawn(Party::REPUBLICAN, "R3", 4);

        for (auto &t : threads)
            t.join();
    };

    Bathroom b1;
    run_test(b1, "Batch + Turn-Based Fairness");

    BathroomFIFO b2;
    run_test(b2, "FIFO");

    {
        BathroomGated b3;
        auto f1 = b3.enter(Party::DEMOCRAT,    "D1", 3);
        auto f2 = b3.enter(Party::DEMOCRAT,    "D2", 4);
        auto f3 = b3.enter(Party::REPUBLICAN,  "R1", 5);
        auto f4 = b3.enter(Party::REPUBLICAN,  "R2", 3);
        auto f5 = b3.enter(Party::DEMOCRAT,    "D3", 2);
        auto f6 = b3.enter(Party::DEMOCRAT,    "D4", 6);
        auto f7 = b3.enter(Party::REPUBLICAN,  "R3", 4);

        f1.get(); f2.get(); f3.get(); f4.get(); f5.get(); f6.get(); f7.get();
    }
}
