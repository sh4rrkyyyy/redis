#pragma once

#include <string>
#include <string_view>
#include <optional>

namespace redis {

class IStorage {
public:
    virtual ~IStorage() = default;

    virtual std::optional<std::string> Get(std::string_view key) const = 0;
    virtual void Set(std::string_view key, std::string_view value) = 0;
    virtual int64_t Change(std::string_view key, int64_t delta) = 0;
};

}  // namespace redis
