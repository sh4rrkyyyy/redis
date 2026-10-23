#pragma once

namespace cactus::details {
template <typename F>
class ScopedFinalizer {
public:
    explicit ScopedFinalizer(F &&func): func_(std::move(func)) {
    }

    ~ScopedFinalizer() noexcept {
        if (!enabled_) {
            return;
        }

        try {
            func_();
        } catch (...) {
        }
    }

    void Cancel() noexcept {
        enabled_ = false;
    }

    void Enable() noexcept {
        enabled_ = true;
    }

private:
    F func_;
    bool enabled_{true};
};
}  // namespace cactus::details
