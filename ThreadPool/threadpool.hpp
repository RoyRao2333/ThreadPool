//
//  threadpool.hpp
//  ThreadPool
//
//  Created by Roy Rao on 2020/12/2.
//

#ifndef threadpool_hpp
#define threadpool_hpp

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>
using namespace std;
using Task = function<void()>;

#endif /* threadpool_hpp */

class ThreadPool {
// MARK: - init & deinit
public:
    ThreadPool(size_t);
    ~ThreadPool();
    
private:
    // forbid copying
    ThreadPool(const ThreadPool&);
    
// MARK: - foos
public:
    template<typename F, typename... Args>
    auto commit(F&& f, Args&&... args) -> future<decltype(f(args...))>;
    void stop();
    void resume();
    size_t available() { return this->spareThread; }
    
private:
    Task fetchTask();
    void schedual();
    
//  MARK: - attributes
private:
    vector<thread> pool;
    queue<Task> tasks;
    mutex mtx;
    condition_variable cv;
    atomic<bool> isRunning;
    atomic<size_t> capacity;
    atomic<size_t> spareThread;
};



/*! @brief Initialize a pool with given size.. */
inline ThreadPool::ThreadPool(size_t poolSize): isRunning(false), capacity(poolSize), spareThread(poolSize) { }

/*! @brief Wait for all threads to finish thier tasks and destruct. */
inline ThreadPool::~ThreadPool() {
    if (this->isRunning) {
        this->stop();
    }
}

/*! @brief Commit a task to current queue and ramdomly wake one. */
template<typename F, typename... Args>
auto ThreadPool::commit(F&& foo, Args&&... args) -> future<decltype(foo(args...))> {
    using ReturnType = decltype(foo(args...));
    auto task = make_shared<packaged_task<ReturnType()>>(
        bind(forward<F>(foo), forward<Args>(args)...)
    );
    future<ReturnType> result = task->get_future();
    // emplace task to queue
    {
        // lock this block
        unique_lock<mutex> ulock(this->mtx);
        if (!this->isRunning) {
            throw runtime_error("ThreadPool have stopped committing.");
        }
        this->tasks.emplace( [task]() {
            (*task)();
        });
    }
    // wake one to execute
    cv.notify_one();
    return result;
}

/*! @brief Stop accepting tasks. */
void ThreadPool::stop() {
    if (!this->isRunning) {
        return;
    } else {
        this->isRunning = false;
        this->spareThread.store(0);
    }
    {
        unique_lock<mutex> ulock(this->mtx);
        this->isRunning = false;
        // wake all threads to finish their tasks
        this->cv.notify_all();
    }
    for(thread &thd: pool) {
        if (thd.joinable()) {
            // wait for it to finish (only if it can be finished)
            thd.join();
        } else {
            // let it be: thd.detach();
        }
    }
    this->pool.clear();
}

/*! @brief Start accepting tasks. */
void ThreadPool::resume() {
    if (this->isRunning) {
        return;
    } else {
        this->isRunning = true;
        this->spareThread.store(this->capacity.load());
    }
    for (size_t i = 0; i < this->spareThread; i++) {
        this->pool.emplace_back( [this] {
            while (this->isRunning) {
                Task task;
                // fetch one pending task
                {
                    unique_lock<mutex> ulock(this->mtx);
                    // wait for next available task
                    this->cv.wait(ulock, [this] {
                        return !this->isRunning || !this->tasks.empty();
                    });
                    if (!this->isRunning && this->tasks.empty()) {
                        return;
                    }
                    // fetch
                    task = move(this->tasks.front());
                    this->tasks.pop();
                }
                this->spareThread--;
                task();
                this->spareThread++;
            }
        });
    }
}
