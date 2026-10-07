#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <chrono>

enum class Party { DEMOCRAT, REPUBLICAN, NONE };

class Bathroom {
    static constexpr int MAX_OCCUPANCY = 3;

    std::mutex mu_;
    std::condition_variable cv_;

    Party current_party_ = Party::NONE;
    int occupancy_ = 0;
    int waiting_d_ = 0, waiting_r_ = 0;
    Party turn_ = Party::DEMOCRAT;

public:
    void enter(Party party, const std::string& name, int duration_sec) {
        // --- ENTRY ---
        {
            std::unique_lock<std::mutex> lock(mu_);

            // Register as waiting BEFORE the wait loop
            if (party == Party::DEMOCRAT) waiting_d_++;
            else waiting_r_++;

            cv_.wait(lock, [&] {
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
            if (party == Party::DEMOCRAT) waiting_d_--;
            else waiting_r_--;

            current_party_ = party;
            occupancy_++;
            std::cout << name << " enters (" << occupancy_ << " inside)\n";
        }

        // --- USE BATHROOM (no lock held) ---
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

        // --- EXIT ---
        {
            std::lock_guard<std::mutex> lock(mu_);
            occupancy_--;
            std::cout << name << " leaves (" << occupancy_ << " inside)\n";

            if (occupancy_ == 0) {
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
class BathroomFIFO {
    static constexpr int MAX_OCCUPANCY = 3;

    std::mutex mu_;
    std::condition_variable cv_;

    Party current_party_ = Party::NONE;
    int occupancy_ = 0;
    int next_ticket_ = 0;   // next ticket to be served
    int ticket_counter_ = 0; // next ticket to be issued

public:
    void enter(Party party, const std::string& name, int duration_sec) {
        {
            std::unique_lock<std::mutex> lock(mu_);
            int my_ticket = ticket_counter_++;

            cv_.wait(lock, [&] {
                bool room = occupancy_ < MAX_OCCUPANCY;
                bool compatible = (current_party_ == party || current_party_ == Party::NONE);
                bool my_turn = (my_ticket == next_ticket_);
                return room && compatible && my_turn;
            });

            current_party_ = party;
            occupancy_++;
            next_ticket_++;
            std::cout << name << " enters (" << occupancy_ << " inside)\n";
        }

        cv_.notify_all();

        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

        {
            std::lock_guard<std::mutex> lock(mu_);
            occupancy_--;
            std::cout << name << " leaves (" << occupancy_ << " inside)\n";
            if (occupancy_ == 0) current_party_ = Party::NONE;
        }
        cv_.notify_all();
    }
};

int main() {
    auto run_test = [](auto& bathroom, const std::string& label) {
        std::cout << "=== " << label << " ===\n";
        std::vector<std::thread> threads;

        auto spawn = [&](Party p, std::string name, int dur) {
            threads.emplace_back([&bathroom, p, name, dur] {
                bathroom.enter(p, name, dur);
            });
        };

        spawn(Party::DEMOCRAT,   "D1", 3);
        spawn(Party::DEMOCRAT,   "D2", 4);
        spawn(Party::REPUBLICAN, "R1", 5);
        spawn(Party::REPUBLICAN, "R2", 3);
        spawn(Party::DEMOCRAT,   "D3", 2);
        spawn(Party::DEMOCRAT,   "D4", 6);
        spawn(Party::REPUBLICAN, "R3", 4);

        for (auto& t : threads) t.join();
        std::cout << "Done.\n\n";
    };

    Bathroom b1;
    run_test(b1, "Batch + Turn-Based Fairness");

    BathroomFIFO b2;
    run_test(b2, "FIFO");
}
