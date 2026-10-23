#pragma once

#include "cactus/net/address.h"
#include "io/socket.h"
#include "query-builder/query_builder.h"
#include "storage.h"
#include "io/async_listener.h"
#include "group.h"
#include "query-parser/parser.h"

#include <memory>

#include <cactus/cactus.h>

namespace redis {

class Server {

public:
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(const cactus::SocketAddress& address, std::unique_ptr<IStorage> storage)
        : addr_(address), storage_(std::move(storage)), listener_(addr_) {
    }

    Task<Unit> Run();
    Task<Unit> Stop();
    const cactus::SocketAddress& GetAddress() const {
        return listener_.Address();
    }

private:
    Task<Unit> Handle(std::unique_ptr<Socket> socket);
    void ProcessReq(RespValue const&, QueryBuilder&);

    cactus::SocketAddress addr_;
    std::unique_ptr<IStorage> storage_;
    AsyncSocketListener listener_;
    CoroutineGroup group_;
    bool stopped_{false};
};

}  // namespace redis
