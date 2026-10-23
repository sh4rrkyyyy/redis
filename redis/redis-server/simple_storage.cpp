#include "simple_storage.h"
#include <string>
#include "error.h"
namespace {

bool IsOverflow(int64_t a, int64_t b) {
    if (b > 0 && a > std::numeric_limits<int64_t>::max() - b) {
        return true;
    }

    if (b < 0 && a < std::numeric_limits<int64_t>::min() - b) {
        return true;
    }

    return false;
}

}  // namespace
namespace redis {

std::optional<std::string> SimpleStorage::Get(std::string_view key) const {
    auto it = storage_.find(std::string{key});
    if (it != storage_.end()) {
        return it->second;
    }
    return {};
}

void SimpleStorage::Set(std::string_view key, std::string_view value) {
    storage_[std::string{key}] = value;
}

int64_t SimpleStorage::Change(std::string_view key, int64_t delta) {
    auto it = storage_.find(std::string{key});
    if (it != storage_.end()) {
        size_t pos{0};
        auto value = std::stol(it->second, &pos);
        if (pos != it->second.size() || IsOverflow(value, delta)) {
            throw RedisError{"Change"};
        }
        value += delta;
        it->second = std::to_string(value);
        return value;
    }
    storage_[std::string{key}] = std::to_string(delta);
    return delta;
}

}  // namespace redis
