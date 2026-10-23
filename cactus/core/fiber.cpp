#include "fiber.h"

#include "scheduler.h"
#include "exceptions.h"
#include "group.h"

#include <glog/logging.h>

namespace cactus {

constexpr int kStackSize = 128 * 1024;

thread_local FiberImpl* this_fiber = nullptr;

TimeoutGuard::TimeoutGuard(const Duration& duration)
    : Timer(CHECK_NOTNULL(this_scheduler)->Timers()), fiber_(CHECK_NOTNULL(this_fiber)) {
    Activate(this_scheduler->Now() + duration);
}

void TimeoutGuard::OnExpired() {
    fiber_->NotifyTimeout();
}

FiberImpl::FiberImpl(const std::function<void()>& fn)
    : where("starting"),
      // NOTE: avoid zero-initializing stack.
      stack_(new char[kStackSize]),
      context_{stack_.get(), kStackSize},
      run_(fn) {
    context_.entry = this;
}

FiberImpl::~FiberImpl() {
}

void FiberImpl::NotifyTimeout() {
    timedout_ = true;
    if (!cancel_guarded_) {
        Unpark();
    }
}

void FiberImpl::Cancel() {
    CHECK(state_ != FiberState::Dead);
    canceled_ = true;
    if (!cancel_guarded_) {
        Unpark();
    }
}

void FiberImpl::Unpark() {
    CHECK(state_ != FiberState::Dead);
    if (!ListElement<RunQueueTag>::IsLinked()) {
        this_scheduler->run_queue_.PushBack(this);
    }
}

void FiberImpl::SetGroup(Group* g) {
    CHECK(!group_);
    group_ = g;
}

SavedContext* FiberImpl::Run() {
    where = nullptr;

    try {
        state_ = FiberState::Running;

        if (group_) {
            try {
                run_();
            } catch (const std::exception& ex) {
                error_ = std::current_exception();
            }
        } else {
            run_();
        }
    } catch (const FiberCanceledException&) {
    }

    run_ = nullptr;
    state_ = FiberState::Dead;
    if (waiter_) {
        waiter_->Unpark();
    }

    return &this_scheduler->main_context_;
}

void FiberImpl::Join() {
    CHECK(!waiter_);
    waiter_ = this_fiber;
    Park("join", [this] { waiter_ = nullptr; });
}

bool FiberImpl::Cleanup() {
    stack_.reset();
    if (group_) {
        group_->OnFinished(this, error_);
        return true;
    }

    return false;
}

Fiber::Fiber(const std::function<void()>& fn) {
    impl_ = new FiberImpl(fn);
    impl_->Unpark();
}

Fiber::~Fiber() {
    if (impl_ && impl_->state_ != FiberState::Dead) {
        impl_->Cancel();

        FiberCancelGuard guard;
        impl_->Join();
    }

    delete impl_;
    impl_ = nullptr;
}

Fiber::Fiber(Fiber&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

Fiber& Fiber::operator=(Fiber&& other) noexcept {
    this->~Fiber();
    new (this) Fiber(std::move(other));
    return *this;
}

}  // namespace cactus
