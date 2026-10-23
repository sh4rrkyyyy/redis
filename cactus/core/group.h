#pragma once

#include "fiber.h"

namespace cactus {

class Group {
public:
    ~Group();

protected:
    List<GroupTag> fibers_;
    FiberImpl* joiner_ = nullptr;

    virtual void OnFinished(FiberImpl* fiber, std::exception_ptr ex);
    void DoSpawn(std::function<void()> fn);

    friend class FiberImpl;
};

class ServerGroup : public Group {
public:
    void Spawn(std::function<void()> fn);
};

class WaitGroup : public Group {
public:
    void Spawn(std::function<void()> fn);
    void Wait();

private:
    std::exception_ptr first_error_;
    FiberImpl* waiter_ = nullptr;

    virtual void OnFinished(FiberImpl* fiber, std::exception_ptr ex) override;
};

}  // namespace cactus
