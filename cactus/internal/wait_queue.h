#pragma once

#include <cactus/internal/intrusive_list.h>

namespace cactus {

class FiberImpl;

class WaitQueue {
public:
    void Wait();

    void NotifyOne();

    void NotifyAll();

private:
    struct WaiterTag {};

    struct Waiter : public ListElement<WaiterTag> {
        FiberImpl *fiber = nullptr;
        bool wakeup = false;
    };

    List<WaiterTag> waiters_;
};

}  // namespace cactus
