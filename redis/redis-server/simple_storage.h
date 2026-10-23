#pragma once

#include "storage.h"

#include <map>
#include <unordered_set>
#include <variant>
#include <deque>
#include <mutex>

namespace redis {

class SimpleStorage : public IStorage {
public:
    std::optional<std::string> Get(std::string_view key) const override;
    void Set(std::string_view key, std::string_view value) override;
    int64_t Change(std::string_view key, int64_t delta) override;

private:
    std::map<std::string, std::string> storage_;
};

}  // namespace redis
