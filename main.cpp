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

//! Очередь для пула потоков.
template<class T>
class TSQueue {
public:

    //! Помещает задачу в очередь.
    void push(T task) {
        std::lock_guard<std::mutex> lk(mux);
        queue.push(task);
        cv.notify_one();
    };

    //! Получает задачу из очереди, если есть возможность.
    bool pop(T& t) {
        std::unique_lock<std::mutex> ul(mux);
        cv.wait(ul, [this]{ return aborted.load() || !queue.empty(); });
        if (aborted.load()) {
            return false;
        }
        t = queue.front();
        queue.pop();
        ul.unlock();
        return true;
    };

    //! Отключает очередь.
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


//! Объект пула потоков.
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
            th.join();
        }
        abort_flag.store(true);
    }

    //! Метод, выполняющий задачу.
    void work() {
        while(!abort_flag.load()) {
            std::cout << "wait..." << std::endl;
            T task;
            bool res = task_queue.pop(task);
            if (res == true) {
                std::cout << "execute..." << std::endl;
                task();
            }
        }
    };

    //! Помещает задачу в очередь.
    void submit(T fn) {
        task_queue.push(fn);
    };

    //! Завершает работу пула потоков.
    void abort() {
        abort_flag.store(true);
        task_queue.abort();
    }

private:
    std::atomic<bool> abort_flag{false};
    std::vector<std::thread> threads;
    TSQueue<T> task_queue;
};


//! Выполняемая структура задачи.
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
    ThreadPull<std::function<void()>> pool(4);

    for (int i = 0; i < 10; ++i) {
        std::function<void()> fn1 = task(1, 10);
        std::function<void()> fn2 = task(2, 10);
        pool.submit(fn1);
        pool.submit(fn2);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Через 5 секунд посылаем сигнал на остановку пула потоков.
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "sending abort signal" << std::endl;
    pool.abort();

    return 0;
}
