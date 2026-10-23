#include "mutex.h"

#include <cactus/core/exceptions.h>
#include <cactus/core/guard.h>

#include <glog/logging.h>

namespace cactus {

MutexGuard::MutexGuard(Mutex& m) : mutex_(&m) {
    Lock();
}

MutexGuard::~MutexGuard() {
    if (locked_) {
        Unlock();
    }
}

void MutexGuard::Lock() {
    mutex_->Lock();
    locked_ = true;
}

void MutexGuard::Unlock() {
    mutex_->Unlock();
    locked_ = false;
}

void Mutex::Lock() {
    while (locked_) {
        lockers_.Wait();
    }

    locked_ = true;
}

void Mutex::Unlock() {
    CHECK(locked_);

    locked_ = false;
    lockers_.NotifyOne();
}

void CondVar::NotifyOne() {
    waiters_.NotifyOne();
}

void CondVar::NotifyAll() {
    waiters_.NotifyAll();
}

void CondVar::Wait(MutexGuard& guard) {
    guard.Unlock();

    waiters_.Wait();

    auto pass_wakeup = cactus::MakeGuard([&] { NotifyOne(); });

    guard.Lock();

    pass_wakeup.Dismiss();
}

}  // namespace cactus
