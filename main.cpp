#include <iostream>

#include <functional>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

using namespace std;

template<class T>
class TSQueue {
public:

    void push(T task) {
        std::lock_guard<std::mutex> lk(mux);
        queue.push(task);
        cv.notify_one();
    };

    T pop() {
        std::unique_lock<std::mutex> ul(mux);
        cv.wait(ul, [this]{ return aborted.load() || !queue.empty(); });
        if (aborted.load()) {
            return nullptr;
        }
        T task = queue.front();
        queue.pop();
        ul.unlock();
        return task;
    };

    void abort() {
        aborted.store(true);
        cv.notify_all();
    }

private:
    std::atomic<bool> aborted;
    std::queue<T> queue;
    std::mutex mux;
    std::condition_variable cv;
};


template<class T>
class ThreadPull {
public:
    ThreadPull(int num) {
        for (int i = 0; i < num; ++i) {
            threads.emplace_back(&ThreadPull::work, this);
        }
    }

    ~ThreadPull() {
        for (auto& th : threads) {
            th.detach();
        }
        abort_flag.store(true);
    }

    void work() {
        while(!abort_flag.load()) {
            std::cout << "wait..." << std::endl;
            T task = task_queue.pop();
            if (task != nullptr) {
                std::cout << "execute..." << std::endl;
                task();
            }
        }
    };

    void submit(T fn) {
        task_queue.push(fn);
    };

    void abort() {
        task_queue.abort();
        abort_flag.store(true);
    }

private:
    std::atomic<bool> abort_flag{false};
    std::vector<std::thread> threads;
    TSQueue<T> task_queue;
};


struct task {
    task(int x, int delay):x(x),delay(delay){}
    int x;
    int delay;

    void operator()() {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    std::cout << "func executed in: " << std::this_thread::get_id() << " " << x << std::endl;
    }
};

int main()
{
    std::function<void()> fn1 = task(200, 200);
    std::function<void()> fn2 = task(150, 150);
    std::function<void()> fn3 = task(50, 50);
    std::function<void()> fn4 = task(10, 10);

    ThreadPull<std::function<void()>> pool(4);

    pool.submit(fn1);
    pool.submit(fn2);
    pool.submit(fn3);
    pool.submit(fn4);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    pool.abort();

    return 0;
}
