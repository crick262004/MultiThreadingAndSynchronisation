#include <iostream>
#include <thread>
#include <mutex>

// A regular std::mutex deadlocks if the same thread locks it twice.
// std::recursive_mutex allows the SAME thread to lock it multiple times,
// as long as it unlocks the same number of times.

// Use case: when a public method that locks the mutex calls another
// public method that also locks the mutex.

class BankAccount {
private:
    double balance;
    std::recursive_mutex mtx;

public:
    BankAccount(double initial) : balance(initial) {}

    void deposit(double amount) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        balance += amount;
        std::cout << "Deposited " << amount << ", balance: " << balance << std::endl;
    }

    void withdraw(double amount) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        if (balance >= amount) {
            balance -= amount;
            std::cout << "Withdrew " << amount << ", balance: " << balance << std::endl;
        } else {
            std::cout << "Insufficient funds for withdrawal of " << amount << std::endl;
        }
    }

    // This method calls withdraw() and deposit(), both of which lock the mutex.
    // With a regular std::mutex, this would deadlock on the second lock.
    // With recursive_mutex, the same thread can re-enter the lock.
    void transfer(BankAccount& to, double amount) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        if (balance >= amount) {
            withdraw(amount);
            to.deposit(amount);
        } else {
            std::cout << "Insufficient funds for transfer of " << amount << std::endl;
        }
    }

    double getBalance() {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        return balance;
    }
};

int main() {
    BankAccount alice(1000);
    BankAccount bob(500);

    std::thread t1(&BankAccount::transfer, &alice, std::ref(bob), 200);

    std::thread t2([&] {
        bob.deposit(300);
    });

    std::thread t3([&] {
        alice.withdraw(100);
    });

    t1.join();
    t2.join();
    t3.join();

    std::cout << "\nFinal balances:" << std::endl;
    std::cout << "Alice: " << alice.getBalance() << std::endl;
    std::cout << "Bob: " << bob.getBalance() << std::endl;

    return 0;
}
