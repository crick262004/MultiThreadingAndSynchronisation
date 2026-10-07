// LeetCode 1242 - Web Crawler Multithreaded
#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
using namespace std;

class HtmlParser {
public:
    vector<string> getUrls(string url) {
        return {};
    }
};

class Solution {
    string hostname;
    unordered_set<string> visited;
    queue<string> taskQueue;
    mutex mtx;
    condition_variable cv, doneCv;
    atomic<int> inFlight{0};
    bool stop = false;

    string getHostname(const string& url) {
        int start = 7;
        int end = url.find('/', start);
        if (end == string::npos) return url.substr(start);
        return url.substr(start, end - start);
    }

    void worker(HtmlParser& htmlParser) {
        while (true) {
            string url;
            {
                unique_lock<mutex> lock(mtx);
                cv.wait(lock, [this]() { return !taskQueue.empty() || stop; });
                if (stop && taskQueue.empty()) return;
                url = taskQueue.front();
                taskQueue.pop();
            }

            vector<string> neighbors = htmlParser.getUrls(url);

            {
                lock_guard<mutex> lock(mtx);
                for (const string& neighbor : neighbors) {
                    if (getHostname(neighbor) == hostname && !visited.count(neighbor)) {
                        visited.insert(neighbor);
                        taskQueue.push(neighbor);
                        inFlight.fetch_add(1);
                        cv.notify_one();
                    }
                }
            }

            if (inFlight.fetch_sub(1) == 1) {
                doneCv.notify_one();
            }
        }
    }

public:
    vector<string> crawl(string startUrl, HtmlParser htmlParser) {
        hostname = getHostname(startUrl);
        stop = false;
        visited.clear();
        visited.insert(startUrl);
        taskQueue.push(startUrl);
        inFlight.store(1);

        vector<thread> threads;
        for (int i = 0; i < 5; i++) {
            threads.emplace_back(&Solution::worker, this, ref(htmlParser));
        }

        {
            unique_lock<mutex> lock(mtx);
            doneCv.wait(lock, [this]() { return inFlight.load() == 0; });
            stop = true;
        }
        cv.notify_all();

        for (auto& t : threads) {
            t.join();
        }

        return vector<string>(visited.begin(), visited.end());
    }
};

int main() {
    HtmlParser htmlParser;
    Solution sol;
    vector<string> result = sol.crawl("http://news.yahoo.com/news/topics/", htmlParser);
    for (const string& url : result) {
        cout << url << endl;
    }
    return 0;
}
