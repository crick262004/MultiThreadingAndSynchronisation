class FooBar {
private:
    int n;
    std::mutex m;
    std::condition_variable cv;
    int turn = 0;
public:
    FooBar(int n) {
        this->n = n;
    }

    void foo(function<void()> printFoo) {
        
        for (int i = 0; i < n; i++) {
            std::unique_lock l(m);
            cv.wait(l, [&](){return turn == 0;});
        	// printFoo() outputs "foo". Do not change or remove this line.
        	printFoo();
            turn = 1;
            cv.notify_one();
        }
    }

    void bar(function<void()> printBar) {
        
        for (int i = 0; i < n; i++) {
            std::unique_lock l(m);
            cv.wait(l, [&](){return turn == 1;});
        	// printFoo() outputs "foo". Do not change or remove this line.
        	printBar();
            turn = 0;
            cv.notify_one();
        }
    }
};
