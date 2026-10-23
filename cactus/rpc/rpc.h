#pragma once

#include <memory>
#include <string>
#include <exception>

#include <google/protobuf/descriptor.h>

#include <cactus/cactus.h>

namespace cactus {

class IRpcChannel {
public:
    virtual ~IRpcChannel() = default;

    virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                            const google::protobuf::Message& request,
                            google::protobuf::Message* response) = 0;
};

class IRpcService : public IRpcChannel {
public:
    virtual const google::protobuf::ServiceDescriptor* ServiceDescriptor() = 0;
};

class RpcCallError : public std::exception {
public:
    explicit RpcCallError(std::string error) : error_{std::move(error)} {
    }

    const char* what() const noexcept override {
        return error_.c_str();
    }

private:
    std::string error_;
};

class SimpleRpcChannel : public IRpcChannel {
public:
    explicit SimpleRpcChannel(const SocketAddress& server) : server_{server} {
    }

    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    const google::protobuf::Message& request,
                    google::protobuf::Message* response) override;

private:
    SocketAddress server_;
};

class SimpleRpcServer {
public:
    explicit SimpleRpcServer(const SocketAddress& at) : lsn_{ListenTCP(at)} {
    }

    void Register(std::unique_ptr<IRpcService> service) {
        const auto& name = service->ServiceDescriptor()->full_name();
        if (!services_.emplace(name, std::move(service)).second) {
            throw std::runtime_error{"Service already registered"};
        }
    }

    const SocketAddress& GetAddress() const {
        return lsn_->Address();
    }

    void Serve();

private:
    std::unique_ptr<IListener> lsn_;
    std::unordered_map<std::string, std::unique_ptr<IRpcService>> services_;
    ServerGroup group_;
};

}  // namespace cactus
