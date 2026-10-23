#pragma once

#include "fiber.h"
#include "timer.h"
#include "exceptions.h"

#include <cactus/io/writer.h>
#include <cactus/net/poller.h>
#include <cactus/net/address.h>

namespace cactus {

class Scheduler;

extern thread_local Scheduler* this_scheduler;

// NOTE(prime@): Work around gdb refusing accessing TLS variables on my system.
FiberImpl* GetCurrentFiber();
Scheduler* GetCurrentScheduler();

void Yield();
void SleepFor(Duration duration);
void SleepUntil(TimePoint time_point);

class Scheduler {
public:
    Scheduler(Scheduler&&) = delete;

    Scheduler();

    void Run(const std::function<void()>& fn);

    TimePoint Now() {
        return now_;
    }

    void FreezeTime() {
        time_freezed_ = true;
    }

    void AdvanceTime(Duration delta) {
        now_ += delta;
    }

    void UnfreezeTime() {
        time_freezed_ = false;
    }

    TimerHeap* Timers() {
        return &timers_;
    }

    Poller poller;

private:
    TimePoint now_;
    bool time_freezed_ = false;

    FiberImpl* main_fiber_ = nullptr;

    TimerHeap timers_;
    List<RunQueueTag> run_queue_;
    List<AllFibersTag> all_fibers_;

    SavedContext main_context_;

    template <class Cleanup>
    friend void Park(const char* where, const Cleanup& cleanup);
    friend void RunFiber(FiberImpl* fiber);
    friend class FiberImpl;
    friend void PrintCactus(IBufferedWriter*);

    void RunLoop() noexcept;
};

template <class Cleanup>
void Park(const char* where, const Cleanup& cleanup) {
    if (!this_fiber) {
        throw std::runtime_error{"this_fiber is null"};
    }
    auto* f = this_fiber;

    auto check = [&] {
        if (!f->cancel_guarded_) {
            if (f->canceled_) {
                this_scheduler->run_queue_.Erase(this_fiber);
                cleanup();
                throw FiberCanceledException{};
            }

            if (f->timedout_) {
                this_scheduler->run_queue_.Erase(this_fiber);
                f->timedout_ = false;
                cleanup();
                throw TimeoutException{where};
            }
        }
    };

    check();

    f->where = where;
    this_scheduler->main_context_.Activate(&f->context_);
    f->where = nullptr;

    check();

    cleanup();
}

}  // namespace cactus
