#pragma once

#include "resp_types.h"

#include <string>
#include <vector>
#include <memory>
#include <span>

namespace redis {

struct RespValue {
    ERespType Type() const {
        return type;
    }
    const auto& AsArray() const {
        return arr;
    }
    size_t ArraySize() const {
        return AsArray().size();
    }
    const std::string& AsString() const {
        return strVal;
    }
    int64_t AsInteger() const {
        return intVal;
    }

    ERespType type;
    int64_t intVal{};
    std::string strVal{};
    std::vector<std::unique_ptr<RespValue>> arr{};
};

class QueryParser {
    using ConstView = std::span<const std::byte>;

public:
    RespValue Parse(ConstView& v);
};

}  // namespace redis
