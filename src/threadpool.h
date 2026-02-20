#ifndef THREADPOOL_H_INCLUDED
#define THREADPOOL_H_INCLUDED

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;

public:
    explicit ThreadPool(int n) {
        workers.reserve(n);
        for (int i = 0; i < n; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto &w : workers) w.join();
    }

    template<typename F>
    std::future<std::invoke_result_t<F>> submit(F&& f) {
        using R = std::invoke_result_t<F>;
        auto pt = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> fut = pt->get_future();
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.emplace([pt]() { (*pt)(); });
        }
        cv.notify_one();
        return fut;
    }
};

#endif
