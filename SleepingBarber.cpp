// Semaphores approach I made:
#include <functional>
#include <mutex>
#include <semaphore>

using namespace std;

class SleepingBarber {
private:
    int total_chairs;
    int occupied_seats;
    mutex mtx;                               // Protects the occupied_seats counter
    counting_semaphore<1000> customers_sem;  // Barber blocks here when sleeping (initial: 0)
    counting_semaphore<1000> barbers_sem;    // Customers block here waiting for their turn (initial: 0)

public:
    // We use the constructor initializer list to set up our semaphores
    SleepingBarber(int n) 
        : total_chairs(n), occupied_seats(0), customers_sem(0), barbers_sem(0) {}

    void barber(function<void()> cutHair) {
        while (true) {
            // 1. Sleep until a customer arrives (decrements customers_sem)
            customers_sem.acquire();

            // 2. A customer is here! Let's free up their waiting room chair
            mtx.lock();
            occupied_seats--;
            
            // 3. Signal the waiting customer that the barber chair is ready
            barbers_sem.release(); 
            mtx.unlock();

            // 4. Perform the haircut action
            cutHair(); 
        }
    }

    void customer(int customerId, function<void()> getHairCut, function<void()> leaveShop) {
        mtx.lock();
        
        // 1. Check if the waiting room is entirely full
        if (occupied_seats == total_chairs) {
            mtx.unlock();
            leaveShop(); // No room, walk out immediately
            return;
        }

        // 2. Sit down in a waiting room chair
        occupied_seats++;
        
        // 3. Signal the barber that a customer is ready (wakes him up if sleeping)
        customers_sem.release(); 
        mtx.unlock();

        // 4. Wait in the waiting room until the barber signals your turn
        barbers_sem.acquire();
        
        // 5. Walk up and get the haircut
        getHairCut();
    }
};

// Condition Variables and an explicit queue approach I found:
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>

std::mutex mtx;
std::condition_variable cv;
std::vector<int> waiting;
const int NUM_CHAIRS = 3;

void barber() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        std::cout << "Barber is sleeping..." << std::endl;
        cv.wait(lock, []{ return !waiting.empty(); });
        int cust = waiting.front();
        waiting.erase(waiting.begin());
        std::cout << "Barber cutting hair of customer " << cust << std::endl;
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(rand() % 3 + 1));
        std::cout << "Barber finished with customer " << cust << std::endl;
        lock.lock();
        cv.notify_one();
    }
}

void customer(int i) {
    std::this_thread::sleep_for(std::chrono::seconds(rand() % 4 + 1));
    std::unique_lock<std::mutex> lock(mtx);
    if (waiting.size() < NUM_CHAIRS) {
        waiting.push_back(i);
        std::cout << "Customer " << i << " waiting" << std::endl;
        lock.unlock();
        cv.notify_one();
        lock.lock();
        cv.wait(lock, []{ return waiting.front() != i; });
        std::cout << "Customer " << i << " got haircut" << std::endl;
    } else {
        std::cout << "Customer " << i << " left (no seat)" << std::endl;
    }
}

int main() {
    std::thread t1(barber);
    for (int i = 0; i < 5; ++i) {
        std::thread t(customer, i);
        t.detach();
    }
    t1.join();
    return 0;
}
How is it different to yours?
