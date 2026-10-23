#pragma once

#include <exception>
#include <string>

namespace cactus {

struct FiberCanceledException {};

class TimeoutException : public std::exception {
public:
    TimeoutException(const char* where) : what_("timeout: ") {
        what_ += where;
    }

    const char* what() const noexcept override {
        return what_.c_str();
    }

private:
    std::string what_;
};

}  // namespace cactus
