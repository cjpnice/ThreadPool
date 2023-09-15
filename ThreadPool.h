//
// Created by SER179 on 2023/9/15.
//

#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <future>

class ThreadPool{
public:
    explicit ThreadPool(size_t thread_num = std::thread::hardware_concurrency()): stop_(false) {
        // 为每个线程分配任务
        for(size_t i = 0; i < thread_num; ++i) {
            works_.emplace_back([this](){
                for (;;) {
                    std::function<void()> task;
                    {
                        // 操作共享变量加锁
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        // 线程池相当于消费者，处理任务队列中的任务，当任务队列中没有任务时等待或者stop_标志为true的时候直接放行
                        this->condition_.wait(lock, [this](){
                            return this->stop_ || !this->tasks_.empty();
                        });
                        // 线程池为空且stop_为true，证明线程池结束，退出线程
                        if (this->stop_ && this->tasks_.empty()) {
                            return;
                        }
                        // 从任务队列中取出任务
                        task = std::move(this->tasks_.front());
                        // 从任务队列中删除取出的任务
                        this->tasks_.pop();
                    }
                    // 执行任务
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            // 加锁
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        // 唤醒所有线程，清理任务
        condition_.notify_all();
        for (std::thread &work: works_) {
            // 等待所以任务完成
            work.join();
        }
    }


    template<typename F, typename... Args>
    auto enqueue(F &&f, Args &&...args) {
        using return_type = std::invoke_result_t<F, Args...>;
        // 完美转发，构造任务仿函数的指针
        auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        // 获得函数执行的future返回
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped Thread pool");
            }
            // 塞入任务队列
            tasks_.emplace([task = std::move(task)]() { (*task)(); });
        }
        // 仅唤醒一个线程，避免无意义的竞争
        condition_.notify_one();
        return res;
    }


private:
    std::vector<std::thread> works_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;


};

#endif //THREADPOOL_THREADPOOL_H
