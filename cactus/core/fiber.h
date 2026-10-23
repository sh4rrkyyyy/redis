#pragma once

#include "cactus/internal/context.h"
#include "cactus/internal/intrusive_list.h"
#include "timer.h"

#include <exception>
#include <memory>
#include <functional>
#include <string>

namespace cactus {

enum FiberState { Ready, Running, Dead, Sleeping, Waiting };

class FiberImpl;
class Group;

extern thread_local FiberImpl* this_fiber;

struct AllFibersTag {};
struct GroupTag {};
struct RunQueueTag {};

class TimeoutGuard : public Timer {
public:
    TimeoutGuard(const Duration& duration);

private:
    FiberImpl* fiber_ = nullptr;

    virtual void OnExpired() override;
};

class FiberCancelTimer : public Timer {
public:
    FiberCancelTimer(TimerHeap* timers, FiberImpl* fiber);

    virtual void OnExpired() override;

private:
    FiberImpl* fiber_;
};

class FiberImpl : private IContextEntry,
                  public ListElement<RunQueueTag>,
                  public ListElement<AllFibersTag>,
                  public ListElement<GroupTag> {
public:
    FiberImpl(const FiberImpl&) = delete;

    explicit FiberImpl(const std::function<void()>& fn);
    virtual ~FiberImpl();

    void NotifyTimeout();
    void Cancel();

    void Unpark();

    const char* where = nullptr;

    bool Cleanup();

    const char* name = nullptr;
    std::string name_storage;

    FiberState State() const {
        return state_;
    }

    void SetGroup(Group* g);

private:
    FiberState state_ = FiberState::Ready;
    std::unique_ptr<char[]> stack_;
    SavedContext context_;

    std::function<void()> run_;
    bool timedout_ = false;
    bool canceled_ = false;
    bool cancel_guarded_ = false;

    std::exception_ptr error_;
    FiberImpl* waiter_ = nullptr;
    void Join();

    Group* group_ = nullptr;

    virtual SavedContext* Run() override;

    template <class Cleanup>
    friend void Park(const char* where, const Cleanup&);
    friend void RunFiber(FiberImpl* fiber);
    friend class FiberCancelGuard;
    friend class Fiber;
};

class FiberCancelGuard {
public:
    FiberCancelGuard() : saved_(this_fiber->cancel_guarded_) {
        this_fiber->cancel_guarded_ = true;
    }

    ~FiberCancelGuard() {
        this_fiber->cancel_guarded_ = saved_;
    }

private:
    bool saved_;
};

class Fiber {
public:
    explicit Fiber(const std::function<void()>& fn);
    ~Fiber();

    Fiber(const Fiber& other) = delete;
    Fiber(Fiber&& other) noexcept;
    Fiber& operator=(const Fiber& other) = delete;
    Fiber& operator=(Fiber&& other) noexcept;

private:
    FiberImpl* impl_ = nullptr;
};

}  // namespace cactus
