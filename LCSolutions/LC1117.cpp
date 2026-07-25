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
