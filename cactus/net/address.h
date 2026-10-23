#pragma once

#include <cstdint>
#include <stdexcept>
#include <optional>
#include <string_view>
#include <string>
#include <cstring>
#include <algorithm>
#include <memory>
#include <system_error>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace cactus {

class SocketAddress;

}  // namespace cactus

namespace std {

template <>
struct hash<sockaddr_in> {
    size_t operator()(const sockaddr_in& sa) const {
        return (static_cast<size_t>(sa.sin_addr.s_addr) << 16) + sa.sin_port;
    }
};

template <>
struct hash<cactus::SocketAddress> {
    inline size_t operator()(const cactus::SocketAddress& address) const;
};

}  // namespace std

namespace cactus {

class SocketAddress {
public:
    SocketAddress() = default;

    SocketAddress(std::string_view address, uint16_t port, bool lookup = false) {
        lookup ? InitFromName(address, port) : InitFromIp(address, port);
    }

    SocketAddress(uint32_t ip, uint16_t port) {
        sockaddr_in sa{};
        sa.sin_addr.s_addr = ip;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        address_ = sa;
    }

    sa_family_t GetFamily() const {
        CheckEmpty();
        return AF_INET;
    }

    socklen_t GetAddress(sockaddr_storage* dest) const {
        CheckEmpty();
        if (!dest) {
            throw std::invalid_argument{"dest must not be null"};
        }
        std::memset(dest, 0, sizeof(sockaddr_storage));
        dest->ss_family = AF_INET;
        *reinterpret_cast<sockaddr_in*>(dest) = *address_;
        return sizeof(*address_);
    }

    void SetFromSockaddr(const sockaddr* address, socklen_t addrlen) {
        if (addrlen < (offsetof(sockaddr, sa_family) + sizeof(address->sa_family))) {
            throw std::invalid_argument{
                "SocketAddress::SetFromSockaddr() called "
                "with length too short for a sockaddr"};
        } else if (address->sa_family == AF_INET) {
            if (addrlen < sizeof(sockaddr_in)) {
                throw std::invalid_argument{
                    "SocketAddress::SetFromSockaddr() called "
                    "with length too short for a sockaddr_in"};
            }
            SetFromSockaddr(reinterpret_cast<const sockaddr*>(address));
        } else {
            throw std::invalid_argument(
                "SocketAddress::SetFromSockaddr() called "
                "with unsupported address type");
        }
    }

    uint16_t GetPort() const {
        CheckEmpty();
        return ntohs(address_->sin_port);
    }

    void SetPort(uint16_t port) {
        CheckEmpty();
        address_->sin_port = htons(port);
    }

    uint32_t GetIp() const {
        CheckEmpty();
        return address_->sin_addr.s_addr;
    }

    bool IsInitialized() const {
        return address_.has_value();
    }

    bool operator==(const SocketAddress& other) const {
        const auto& a = address_;
        const auto& b = other.address_;
        if (a && b) {
            return (a->sin_addr.s_addr == b->sin_addr.s_addr) && (a->sin_port == b->sin_port);
        } else {
            return a.has_value() == b.has_value();
        }
    };

private:
    struct Deleter {
        void operator()(addrinfo* p) const noexcept {
            ::freeaddrinfo(p);
        }
    };

    auto GetAddrInfo(const char* host, const char* port) const {
        addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
#ifdef __APPLE__
        if (host && strcmp(host, "localhost") == 0) {
            hints.ai_family = AF_INET;
        }
#endif

        addrinfo* results;
        if (auto error = getaddrinfo(host, port, &hints, &results)) {
            throw std::system_error{error, std::generic_category(), "failed to resolve address"};
        }
        return std::unique_ptr<addrinfo, Deleter>{results};
    }

    void InitFromName(std::string_view name, uint16_t port) {
        std::string address_str{name};
        auto port_str = std::to_string(port);
        auto info = GetAddrInfo(address_str.c_str(), port_str.c_str());
        SetFromSockaddr(info->ai_addr, info->ai_addrlen);
        address_->sin_port = htons(port);
    }

    void InitFromIp(std::string_view ip, uint16_t port) {
        if (ip.size() >= INET_ADDRSTRLEN) {
            throw std::invalid_argument{"address is too long"};
        }
        char array[INET_ADDRSTRLEN] = {};
        std::ranges::copy(ip, array);

        sockaddr_in sa{};
        if (::inet_pton(AF_INET, array, &sa.sin_addr) < 0) {
            throw std::system_error(errno, std::generic_category());
        };
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        address_ = sa;
    }

    void CheckEmpty() const {
        if (!address_) {
            throw std::runtime_error{"address is empty"};
        }
    }

    void SetFromSockaddr(const sockaddr* address) {
        if (address->sa_family == AF_INET) {
            address_ = *reinterpret_cast<const sockaddr_in*>(address);
        } else {
            throw std::invalid_argument{
                "SocketAddress::SetFromSockaddr() called "
                "with unsupported address type"};
        }
    }

    friend size_t std::hash<SocketAddress>::operator()(const SocketAddress&) const;

    friend std::ostream& operator<<(std::ostream& os, const SocketAddress& address) {
        if (const auto& a = address.address_) {
            char str[INET_ADDRSTRLEN];
            ::inet_ntop(AF_INET, &(a->sin_addr.s_addr), str, INET_ADDRSTRLEN);
            os << str << ':' << ntohs(a->sin_port);
        } else {
            os << "[nullopt]";
        }
        return os;
    }

    std::optional<sockaddr_in> address_;
};

}  // namespace cactus

size_t std::hash<cactus::SocketAddress>::operator()(const cactus::SocketAddress& address) const {
    return std::hash<std::optional<sockaddr_in>>{}(address.address_);
}
