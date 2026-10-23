#pragma once

#include <errno.h>

namespace cactus {

template <class Syscall, class... Args>
inline int RestartEIntr(Syscall syscall, Args... args) {
    int ret;
    do {
        ret = syscall(args...);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

}  // namespace cactus
