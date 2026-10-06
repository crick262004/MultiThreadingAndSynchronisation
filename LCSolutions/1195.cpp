class FizzBuzz {
private:
    int n;

public:
    FizzBuzz(int n) {
        this->n = n;
    }

    // printFizz() outputs "fizz".
    void fizz(function<void()> printFizz) {
        while(true){
            auto curnum = cnt.load(memory_order_acquire);
            if(curnum > n) break;
            if(curnum % 3 == 0 && curnum % 5 != 0)
            {
                printFizz();
                cnt.store(curnum + 1, memory_order_release);
            }
        }
    }

    // printBuzz() outputs "buzz".
    void buzz(function<void()> printBuzz) {
        while(true){
            auto curnum = cnt.load(memory_order_acquire);
            if(curnum > n) break;
            if(curnum % 3 != 0 && curnum % 5 == 0)
            {
                printBuzz();
                cnt.store(curnum + 1, memory_order_release);
            }
        }
    }

    // printFizzBuzz() outputs "fizzbuzz".
	void fizzbuzz(function<void()> printFizzBuzz) {
        while(true){
            auto curnum = cnt.load(memory_order_acquire);
            if(curnum > n) break;
            if(curnum % 3 == 0 && curnum % 5 == 0)
            {
                printFizzBuzz();
                cnt.store(curnum + 1, memory_order_release);
            }
        }
    }

    // printNumber(x) outputs "x", where x is an integer.
    void number(function<void(int)> printNumber) {
        while(true){
            auto curnum = cnt.load(memory_order_acquire);
            if(curnum > n) break;
            if(curnum % 3 != 0 && curnum % 5 != 0)
            {
                printNumber(curnum);
                cnt.store(curnum + 1, memory_order_release);
            }
        }
    }    
    
private:
    atomic<int> cnt{1};
};
