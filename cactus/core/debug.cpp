#include "debug.h"

#include "fiber.h"
#include "scheduler.h"
#include "cactus/io/writer.h"

#include <unistd.h>

#include <glog/logging.h>

namespace cactus {

class SignalSafeWriter final : public IBufferedWriter {
public:
    explicit SignalSafeWriter(int fd) : fd_(fd) {
    }

    virtual MutableView WriteNext() override {
        if (end_ == buffer_.size()) {
            Flush();
        }

        auto chunk = View(buffer_).subspan(end_);
        end_ = buffer_.size();
        return chunk;
    }

    virtual void WriteBackUp(size_t count) override {
        end_ -= count;
    }

    virtual void Flush() override {
        if (end_ != 0) {
            write(fd_, buffer_.data(), end_);
            end_ = 0;
        }
    }

private:
    int fd_ = -1;
    std::array<char, 512> buffer_ = {};
    size_t end_ = 0;
};

void SetFiberName(const char* name) {
    CHECK_NOTNULL(this_fiber);
    this_fiber->name = name;
}

void SetFiberName(const std::string& name) {
    CHECK_NOTNULL(this_fiber);
    this_fiber->name_storage = name;
    this_fiber->name = this_fiber->name_storage.c_str();
}

}  // namespace cactus
