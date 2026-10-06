class H2O {
private:
    std::mutex m;
    std::condition_variable cv;
    int step = 0;
public:
    H2O() {
        
    }

    void hydrogen(function<void()> releaseHydrogen) {
        std::unique_lock l(m);
        cv.wait(l, [&](){
            return step <2;
        });
        releaseHydrogen();
        step++;
        cv.notify_all();
    }

    void oxygen(function<void()> releaseOxygen) {
        std::unique_lock l(m);
        cv.wait(l, [&](){
            return step == 2;
        });
        releaseOxygen();
        step = 0;
        cv.notify_all();
    }
};

// semaphore solution:
class H2O {
    counting_semaphore<2> h{2};
    counting_semaphore<1> o{0};
    mutex m;
public:
    H2O() {
        
    }

    void hydrogen(function<void()> releaseHydrogen) {
        h.acquire();
        // releaseHydrogen() outputs "H". Do not change or remove this line.
        releaseHydrogen();
        o.release();
    }

    void oxygen(function<void()> releaseOxygen) {
        m.lock();
        o.acquire();
        o.acquire();
        // releaseOxygen() outputs "O". Do not change or remove this line.
        releaseOxygen();
        h.release();
        h.release();
        m.unlock();
    }
};