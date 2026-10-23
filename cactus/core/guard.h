#pragma once

#include <functional>

namespace cactus {

class ScopeGuard {
public:
    template <class Func>
    explicit ScopeGuard(Func&& func) : cleanup_{std::forward<Func>(func)} {
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) : cleanup_{std::move(other.cleanup_)} {
        other.Dismiss();
    }
    ScopeGuard& operator=(ScopeGuard&& other) {
        cleanup_ = std::move(other.cleanup_);
        other.Dismiss();
        return *this;
    };

    ~ScopeGuard() {
        if (cleanup_) {
            cleanup_();
        }
    }

    void Dismiss() {
        cleanup_ = std::function<void()>{};
    }

private:
    std::function<void()> cleanup_;
};

template <class Func>
auto MakeGuard(Func&& func) {
    return ScopeGuard{std::forward<Func>(func)};
}

}  // namespace cactus
