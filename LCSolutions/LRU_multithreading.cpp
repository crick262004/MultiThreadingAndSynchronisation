// used Sharding to enable multithreading 
#include <unordered_map>
#include <mutex>
#include <vector>
using namespace std;

// Same as your LRUCache, plus a mutex
class LRUShard {
public:
    class node {
    public:
        int key, val;
        node* next = nullptr;
        node* prev = nullptr;
        node(int _key, int _val) { key = _key; val = _val; }
    };

    node* head = new node(-1, -1);
    node* tail = new node(-1, -1);
    int cap;
    unordered_map<int, node*> m;
    mutex mtx;                                  // one lock per shard

    LRUShard(int capacity) {
        cap = capacity;
        head->next = tail;
        tail->prev = head;
    }

    void addnode(node* newnode) {
        node* temp = head->next;
        newnode->next = temp;
        newnode->prev = head;
        temp->prev = newnode;
        head->next = newnode;
    }

    void deletenode(node* delnode) {
        delnode->prev->next = delnode->next;
        delnode->next->prev = delnode->prev;
    }

    int get(int key) {
        lock_guard<mutex> lock(mtx);            // get changes order, so it needs the lock
        if (m.find(key) == m.end()) return -1;
        node* n = m[key];
        deletenode(n);
        addnode(n);
        return n->val;
    }

    void put(int key, int value) {
        lock_guard<mutex> lock(mtx);
        if (m.find(key) != m.end()) {
            node* n = m[key];
            n->val = value;
            deletenode(n);
            addnode(n);
            return;
        }
        if ((int)m.size() == cap) {
            node* lru = tail->prev;
            m.erase(lru->key);
            deletenode(lru);
            delete lru;                        
        }
        node* n = new node(key, value);
        addnode(n);
        m[key] = n;
    }
};

// Sends each key to one shard
class LRUCache {
public:
    vector<LRUShard*> shards;
    int n;

    LRUCache(int capacity, int numShards = 16) {
        n = numShards;
        int perShard = (capacity + n - 1) / n;  // round up
        for (int i = 0; i < n; i++)
            shards.push_back(new LRUShard(perShard));
    }

    LRUShard* getShard(int key) {
        return shards[((key % n) + n) % n];     // safe for negative keys
        // so we don't apply any extra hashing, just mod the key to get its shard. 
        // This can be improved to avoid key patterns skew-ness.
    }

    int get(int key)            { return getShard(key)->get(key); }
    void put(int key, int val)  { getShard(key)->put(key, val); }
};