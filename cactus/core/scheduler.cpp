#include "scheduler.h"
#include "exceptions.h"

#include <thread>

#include <glog/logging.h>

namespace cactus {

thread_local Scheduler* this_scheduler = nullptr;

void RunFiber(FiberImpl* fiber) {
    CHECK(fiber->state_ != FiberState::Dead);
    CHECK(!this_fiber);

    this_fiber = fiber;
    fiber->context_.Activate(&this_scheduler->main_context_);
    this_fiber = nullptr;
}

void Yield() {
    this_fiber->Unpark();
    Park("yield", [] {});
}

void SleepFor(Duration duration) {
    SleepUntil(CHECK_NOTNULL(this_scheduler)->Now() + duration);
}

FiberImpl* GetCurrentFiber() {
    return this_fiber;
}

Scheduler* GetCurrentScheduler() {
    return this_scheduler;
}

class SleepTimer : public Timer {
public:
    SleepTimer(TimerHeap* timers, FiberImpl* fiber) : Timer(timers), fiber_(fiber) {
    }

    virtual void OnExpired() override {
        fiber_->Unpark();
    }

private:
    FiberImpl* fiber_;
};

void SleepUntil(TimePoint deadline) {
    SleepTimer wakeup{this_scheduler->Timers(), this_fiber};
    wakeup.Activate(deadline);
    Park("sleep", [] {});
}

Scheduler::Scheduler() : now_(std::chrono::steady_clock::now()) {
}

struct CurrentSchedulerGuard {
    explicit CurrentSchedulerGuard(Scheduler* scheduler) {
        CHECK(!this_scheduler);
        this_scheduler = scheduler;
    }

    ~CurrentSchedulerGuard() {
        this_scheduler = nullptr;
    }
};

void Scheduler::RunLoop() noexcept {
    while (main_fiber_->State() != FiberState::Dead) {
        poller.Poll(nullptr);

        if (!time_freezed_) {
            auto next_time_point = std::chrono::steady_clock::now();
            CHECK(now_ <= next_time_point);
            now_ = next_time_point;
        }

        Timer* next_timer = timers_.GetMin();
        while (next_timer && next_timer->GetDeadline() < now_) {
            next_timer->Deactivate();
            next_timer->OnExpired();
            next_timer = timers_.GetMin();
        }

        if (run_queue_.IsEmpty()) {
            Duration timeout = Duration::max();
            if (next_timer) {
                timeout = next_timer->GetDeadline() - now_;
            }

            poller.Poll(&timeout);
            continue;
        }

        CHECK(!run_queue_.IsEmpty());
        auto next = run_queue_.Begin();
        next->Unlink();

        auto next_fiber = next->As<FiberImpl>();
        RunFiber(next_fiber);
        if (next_fiber->State() == FiberState::Dead && next_fiber->Cleanup()) {
            delete next_fiber;
        }
    }
}

void Scheduler::Run(const std::function<void()>& fn) {
    CurrentSchedulerGuard guard(this);

    std::exception_ptr main_error;
    main_fiber_ = new FiberImpl([this, &fn, &main_error] {
        try {
            fn();
        } catch (...) {
            main_error = std::current_exception();
        }
    });
    main_fiber_->Unpark();

    RunLoop();

    CHECK(all_fibers_.IsEmpty());
    delete main_fiber_;
    main_fiber_ = nullptr;

    if (main_error) {
        std::rethrow_exception(main_error);
    }
}

}  // namespace cactus
