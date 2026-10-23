#include "server.h"
#include "query-builder/query_builder.h"
#include "query-parser/parser.h"

namespace redis {

void Server::ProcessReq(RespValue const& resp_value, QueryBuilder& qb) {
    auto const& arr = resp_value.AsArray();
    auto op = arr[0]->AsString();
    if (op == "GET") {
        if (auto res = storage_->Get(arr[1]->AsString())) {
            qb.WriteBulkString(*res);
        } else {
            qb.WriteNullBulkString();
        }
    } else if (op == "SET") {
        auto key = arr[1]->AsString();
        auto value = arr[2]->AsString();
        storage_->Set(key, value);
        qb.WriteBulkString("OK");
    } else if (op == "INCR" || op == "DECR") {
        auto value = storage_->Change(arr[1]->AsString(), op == "INCR" ? 1 : -1);
        qb.WriteInt(value);
    }
}

Task<Unit> Server::Handle(std::unique_ptr<Socket> socket) {
    for (;;) {
        std::array<std::byte, 256> buf;
        auto sz = co_await socket->Read(buf);
        if (sz == 0) {
            break;
        }
        std::span<const std::byte> view{buf.data(), sz};

        QueryBuilder qb;
        try {
            QueryParser qp;
            RespValue resp_value = qp.Parse(view);
            ProcessReq(resp_value, qb);
        } catch (std::exception const& e) {
            qb.WriteError(e.what());
        }
        auto req = qb.Request();
        co_await socket->Write({reinterpret_cast<const std::byte*>(req.data()), req.size()});
    }
    co_return Unit{};
}

Task<Unit> Server::Run() {
    while (!stopped_) {
        auto socket = co_await listener_.Accept();
        group_.Spawn(Handle(std::move(socket)));
    }
    co_return Unit{};
}

Task<Unit> Server::Stop() {
    stopped_ = true;
    listener_.Close();
    co_await group_.Join();
    co_return Unit{};
}

}  // namespace redis
