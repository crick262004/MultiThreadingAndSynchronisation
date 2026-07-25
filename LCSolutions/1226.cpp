class DiningPhilosophers {
private:
    std::deque<std::counting_semaphore<1>> f;
public:
    DiningPhilosophers() {
        for(int i = 0; i<5; i++){
            f.emplace_back(1);
        }
    }

    void wantsToEat(int philosopher,
                    function<void()> pickLeftFork,
                    function<void()> pickRightFork,
                    function<void()> eat,
                    function<void()> putLeftFork,
                    function<void()> putRightFork) {
        int left = philosopher;
        int right = (philosopher + 1)%5;

        int mini = min(left, right);
		f[mini].acquire();
        int maxi = max(left, right);
        f[maxi].acquire();

        pickLeftFork();
        pickRightFork();
        eat();
        putLeftFork();
        putRightFork();
        
        f[mini].release();
        f[maxi].release();
    }
};
