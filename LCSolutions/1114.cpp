class Foo {
private:
    std::mutex m;
    std::condition_variable cv;
    int turn = 1;
public:
    Foo() {

    }

    void first(function<void()> printFirst) {
        std::unique_lock l(m);
        cv.wait(l, [&](){return turn ==1;});
        // printFirst() outputs "first". Do not change or remove this line.
        printFirst();
        turn = 2;
        cv.notify_all();
    }

    void second(function<void()> printSecond) {
        std::unique_lock l(m);
        cv.wait(l, [&](){return turn ==2;});
        // printSecond() outputs "second". Do not change or remove this line.
        printSecond();
        turn = 3;
        cv.notify_all();
    }

    void third(function<void()> printThird) {
        std::unique_lock l(m);
        cv.wait(l, [&](){return turn ==3;});
        // printThird() outputs "third". Do not change or remove this line.
        printThird();
    }
};
