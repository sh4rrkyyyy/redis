#include "poller.h"

#include "cactus/core/debug.h"
#include "cactus/core/scheduler.h"

#ifdef __linux__
#include <sys/epoll.h>
#else
#include <sys/event.h>
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include <system_error>

#include <glog/logging.h>

namespace cactus {

Poller::Poller() {
#ifdef __APPLE__
    epoll_fd_ = kqueue();
#else
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
#endif
    if (epoll_fd_ == -1) {
        throw std::system_error(errno, std::system_category(), "epoll_create1");
    }
#ifdef __APPLE__
    if (int flags = fcntl(epoll_fd_, F_GETFD, 0); flags == -1 || fcntl(epoll_fd_, F_SETFD, flags | FD_CLOEXEC) == -1) {
        throw std::system_error(errno, std::system_category(), "poller fcntl");
    }
#endif
}

Poller::~Poller() {
    int ret = close(epoll_fd_);
    PCHECK(ret == 0) << "close() system call failed";
    epoll_fd_ = -1;

    while (free_list_) {
        auto desc = free_list_;
        free_list_ = free_list_->next;
        delete desc;
    }
}

void Poller::Poll(Duration* timeout) {
    constexpr int kEpollBatchSize = 64;
#ifdef __APPLE__
    struct kevent events[kEpollBatchSize];
#else
    epoll_event events[kEpollBatchSize];
#endif

    int epoll_timeout = 0;
    if (timeout != nullptr) {
        if (*timeout == Duration::max()) {
            epoll_timeout = -1;
        } else {
            epoll_timeout = std::max<int>(
                5, std::chrono::duration_cast<std::chrono::milliseconds>(*timeout).count());
        }
    }
#ifdef __APPLE__
    struct timespec ts;
    if (epoll_timeout != -1) {
        ts.tv_sec = epoll_timeout / 1000;
        ts.tv_nsec = (epoll_timeout % 1000) * 1000000;
    }
    int nevents = kevent(epoll_fd_, nullptr, 0, events, kEpollBatchSize, &ts);
#else
    int nevents = epoll_wait(epoll_fd_, events, kEpollBatchSize, epoll_timeout);
#endif
    if (nevents == -1) {
        if (errno == EINTR) {
            return;
        }

        throw std::system_error(errno, std::system_category(), "epoll_wait");
    }

    for (int i = 0; i < nevents; ++i) {
#ifdef __APPLE__
        FDDesc* desc = static_cast<FDDesc*>(events[i].udata);
        auto read_available = (events[i].filter == EVFILT_READ) || (events[i].flags & EV_EOF);
        auto write_available = (events[i].filter == EVFILT_WRITE) || (events[i].flags & EV_EOF);
#else
        FDDesc* desc = static_cast<FDDesc*>(events[i].data.ptr);
        auto read_available = events[i].events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);
        auto write_available = events[i].events & (EPOLLOUT | EPOLLHUP | EPOLLERR);
#endif

        if (read_available) {
            if (desc->reader) {
                desc->reader->Unpark();
                desc->reader = nullptr;
            }
        }

        if (write_available) {
            if (desc->writer) {
                desc->writer->Unpark();
                desc->writer = nullptr;
            }
        }
    }
}

FDDesc* Poller::NewDesc() {
    if (free_list_) {
        auto desc = free_list_;
        free_list_ = desc->next;
        return desc;
    }

    return new FDDesc{};
}

void Poller::FreeDesc(FDDesc* desc) {
    desc->next = free_list_;
    free_list_ = desc;
}

FDWatcher::FDWatcher(Poller* poller) : poller_(poller) {
}

FDWatcher::~FDWatcher() {
    DCHECK(!desc_);
}

void FDWatcher::Init(int fd) {
    desc_ = poller_->NewDesc();
    desc_->fd = fd;
#ifdef __APPLE__
    struct kevent events[2];
    EV_SET(&events[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, desc_);
    EV_SET(&events[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, desc_);
    int ret = kevent(poller_->epoll_fd_, events, 2, nullptr, 0, nullptr);
#else
    epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    event.data.ptr = desc_;
    int ret = epoll_ctl(poller_->epoll_fd_, EPOLL_CTL_ADD, fd, &event);
#endif
    if (ret != 0) {
        desc_->fd = -1;
        poller_->FreeDesc(desc_);
        throw std::system_error(errno, std::system_category(), "epoll_ctl");
    }
}

void FDWatcher::Reset() {
#ifdef __APPLE__
    struct kevent event;
    EV_SET(&event, desc_->fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(poller_->epoll_fd_, &event, 1, nullptr, 0, nullptr);
    
    EV_SET(&event, desc_->fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(poller_->epoll_fd_, &event, 1, nullptr, 0, nullptr);
#else
    int ret = epoll_ctl(poller_->epoll_fd_, EPOLL_CTL_DEL, desc_->fd, nullptr);
    PCHECK(ret == 0) << "epoll_ctl() failed";
#endif

    desc_->fd = -1;
    if (desc_->writer) {
        desc_->writer->Unpark();
        desc_->writer = nullptr;
    }
    if (desc_->reader) {
        desc_->reader->Unpark();
        desc_->reader = nullptr;
    }

    poller_->FreeDesc(desc_);
    desc_ = nullptr;
}

bool FDWatcher::Wait(bool read) {
    auto& slot = (read ? desc_->reader : desc_->writer);
    CHECK(!slot);

    slot = this_fiber;
    Park(read ? "read" : "write", [&, this] {
        if (desc_) {
            slot = nullptr;
        }
    });

    return !desc_;
}

}  // namespace cactus
