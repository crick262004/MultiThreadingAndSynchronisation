#include <mutex>
#include <condition_variable>
#include <queue>

class BoundedBlockingQueue {
private:
    std::queue<int> q;
    int capacity;
    std::mutex mtx;
    std::condition_variable cv_enqueue;
    std::condition_variable cv_dequeue;

public:
    BoundedBlockingQueue(int capacity) : capacity(capacity) {}
    
    void enqueue(int element) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until queue is not full
        cv_enqueue.wait(lock, [this]() { return q.size() < capacity; });
        
        q.push(element);
        
        // Notify one waiting dequeue thread
        cv_dequeue.notify_one();
    }
    
    int dequeue() {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until queue is not empty
        cv_dequeue.wait(lock, [this]() { return !q.empty(); });
        
        int val = q.front();
        q.pop();
        
        // Notify one waiting enqueue thread
        cv_enqueue.notify_one();
        
        return val;
    }
    
    int size() {
        std::unique_lock<std::mutex> lock(mtx);
        return q.size();
    }
};