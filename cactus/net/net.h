#pragma once

#include "cactus/io/reader.h"
#include "cactus/io/writer.h"
#include "cactus/net/address.h"

namespace cactus {

class IConn : public IReader, public IWriteCloser {
public:
    virtual void CloseWrite() = 0;
    virtual const cactus::SocketAddress& RemoteAddress() const = 0;
    virtual const cactus::SocketAddress& LocalAddress() const = 0;
};

class IDialer {
public:
    virtual ~IDialer() = default;

    virtual std::unique_ptr<IConn> Dial(const std::string& address) = 0;
};

std::unique_ptr<IConn> DialTCP(const cactus::SocketAddress& to);

class IListener {
public:
    virtual ~IListener() = default;

    virtual void Close() = 0;
    virtual const cactus::SocketAddress& Address() const = 0;
    virtual std::unique_ptr<IConn> Accept() = 0;
};

std::unique_ptr<IListener> ListenTCP(const cactus::SocketAddress& at);

class IPacketConn {
public:
    virtual ~IPacketConn() = default;
    virtual void Close() = 0;
    virtual size_t RecvFrom(MutableView buf, cactus::SocketAddress* from) = 0;
    virtual size_t SendTo(ConstView buf, const cactus::SocketAddress& to) = 0;
};

std::unique_ptr<IPacketConn> ListenUDP(const cactus::SocketAddress& at);

}  // namespace cactus
