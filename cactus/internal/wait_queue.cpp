#include "wait_queue.h"

#include <cactus/core/fiber.h>
#include <cactus/core/scheduler.h>
#include <cactus/core/guard.h>

#include <glog/logging.h>

namespace cactus {

void WaitQueue::Wait() {
    CHECK(this_fiber);

    Waiter me;
    me.fiber = this_fiber;

    waiters_.PushBack(&me);

    auto pass_wakeup = cactus::MakeGuard([&] {
        if (me.wakeup) {
            NotifyOne();
        }
    });

    Park("wait_queue", [] {});

    pass_wakeup.Dismiss();
}

void WaitQueue::NotifyOne() {
    if (waiters_.IsEmpty()) {
        return;
    }

    auto first = waiters_.Begin()->As<Waiter>();
    first->Unlink();
    first->wakeup = true;
    first->fiber->Unpark();
}

void WaitQueue::NotifyAll() {
    while (!waiters_.IsEmpty()) {
        NotifyOne();
    }
}

}  // namespace cactus
