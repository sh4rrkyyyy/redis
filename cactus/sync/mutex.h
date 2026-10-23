#pragma once

#include <memory>

#include <cactus/internal/wait_queue.h>

namespace cactus {

class Mutex;

class MutexGuard {
public:
    explicit MutexGuard(Mutex& m);

    ~MutexGuard();

    void Lock();

    void Unlock();

private:
    struct DummyDeleter {
        void operator()(Mutex*) const {
        }
    };

    std::unique_ptr<Mutex, DummyDeleter> mutex_;
    bool locked_ = false;
};

class Mutex {
public:
    Mutex() = default;

    Mutex(const Mutex&) = delete;

    void Lock();

    void Unlock();

private:
    bool locked_ = false;
    WaitQueue lockers_;
};

class CondVar {
public:
    void NotifyOne();

    void NotifyAll();

    void Wait(MutexGuard& guard);

    template <class Condition>
    void WaitFor(MutexGuard& guard, Condition condition) {
        while (!condition()) {
            Wait(guard);
        }
    }

private:
    Mutex* mutex_ = nullptr;
    WaitQueue waiters_;
};

}  // namespace cactus
