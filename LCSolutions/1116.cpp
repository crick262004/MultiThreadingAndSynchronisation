class ZeroEvenOdd {
private:
    int n;
    std::binary_semaphore z = std::binary_semaphore(1);
    std::binary_semaphore o = std::binary_semaphore(0);
    std::binary_semaphore e = std::binary_semaphore(0);
    int x = 0;

public:
    ZeroEvenOdd(int n) {
        this->n = n;
        x = 1;
    }

    // printNumber(x) outputs "x", where x is an integer.
    void zero(function<void(int)> printNumber) {
        for (int i = 0; i < n; i++) {
            z.acquire();
            printNumber(0);
            if (x & 1) {
                o.release();
            } else {
                e.release();
            }
        }
    }

    void even(function<void(int)> printNumber) {
        for (int i = 0; i < n / 2; i++) {
            e.acquire();
            printNumber(x++);
            z.release();
        }
    }

    void odd(function<void(int)> printNumber) {
        for (int i = 0; i < (n + 1) / 2; i++) {
            o.acquire();
            printNumber(x++);
            z.release();
        }
    }
};
