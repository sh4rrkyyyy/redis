#pragma once

#include "cactus/core/fiber.h"

namespace cactus {

struct FDDesc {
    int fd = -1;

    FiberImpl* reader = nullptr;
    FiberImpl* writer = nullptr;

    FDDesc* next = nullptr;
};

class Poller {
public:
    Poller(Poller&&) = delete;

    Poller();
    ~Poller();

    void Poll(Duration* timeout);

private:
    int epoll_fd_ = -1;

    friend class FDWatcher;
    FDDesc* free_list_ = nullptr;

    FDDesc* NewDesc();
    void FreeDesc(FDDesc* desc);
};

class FDWatcher {
public:
    explicit FDWatcher(Poller* poller);
    ~FDWatcher();

    void Init(int fd);
    bool Wait(bool read);
    void Reset();

private:
    Poller* poller_ = nullptr;
    FDDesc* desc_ = nullptr;
};

}  // namespace cactus
