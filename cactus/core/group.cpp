#include "group.h"

#include "scheduler.h"

#include <glog/logging.h>

namespace cactus {

void Group::DoSpawn(std::function<void()> fn) {
    auto fiber = new FiberImpl(fn);
    fiber->Unpark();
    fiber->SetGroup(this);
    fibers_.PushBack(fiber);
}

Group::~Group() {
    if (fibers_.IsEmpty()) {
        return;
    }

    for (auto node = fibers_.Begin(); node != fibers_.End(); node = node->next) {
        auto fiber = node->As<FiberImpl>();
        fiber->Cancel();
    }

    FiberCancelGuard guard;
    joiner_ = CHECK_NOTNULL(this_fiber);
    Park("group", [this] { joiner_ = nullptr; });
}

void LogUnhandledException(std::exception_ptr ex) {
    try {
        std::rethrow_exception(std::move(ex));
    } catch (const std::exception& ex) {
        LOG(ERROR) << "unhandled exception: " << ex.what();
    }
}

void Group::OnFinished(FiberImpl* fiber, std::exception_ptr ex) {
    if (ex) {
        LogUnhandledException(std::move(ex));
    }

    fibers_.Erase(fiber);

    if (fibers_.IsEmpty() && joiner_) {
        joiner_->Unpark();
    }
}

void ServerGroup::Spawn(std::function<void()> fn) {
    DoSpawn(std::move(fn));
}

void WaitGroup::Spawn(std::function<void()> fn) {
    DoSpawn(std::move(fn));
}

void WaitGroup::Wait() {
    if (first_error_) {
        std::rethrow_exception(first_error_);
    }

    if (fibers_.IsEmpty()) {
        return;
    }

    CHECK(!waiter_);
    waiter_ = this_fiber;
    Park("wait_group", [this] { waiter_ = nullptr; });

    if (first_error_) {
        std::rethrow_exception(first_error_);
    }
}

void WaitGroup::OnFinished(FiberImpl* fiber, std::exception_ptr ex) {
    if (!first_error_ && ex) {
        first_error_ = std::move(ex);
        ex = nullptr;

        if (waiter_) {
            waiter_->Unpark();
        }
    }

    Group::OnFinished(fiber, ex);

    if (fibers_.IsEmpty() && waiter_) {
        waiter_->Unpark();
    }
}

}  // namespace cactus
