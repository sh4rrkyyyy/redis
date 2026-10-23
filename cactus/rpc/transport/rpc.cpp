#include "../rpc.h"

#include <cactus/rpc/rpc.pb.h>

namespace cactus {

namespace {

struct FixedHeader {
    uint32_t header_size;
    uint32_t body_size;
};

using Services = std::unordered_map<std::string, std::unique_ptr<IRpcService>>;

void HandleConnection(IConn* client, const Services& services) {
    FixedHeader fixed;
    client->ReadFull(View(fixed));

    std::string header_string(fixed.header_size, '\0');
    std::string body_string(fixed.body_size, '\0');

    client->ReadFull(View(header_string));
    client->ReadFull(View(body_string));

    auto reply_error = [&](const std::string& error) {
        ResponseHeader response;
        response.mutable_rpc_error()->set_message(error);

        header_string = response.SerializeAsString();
        fixed.header_size = header_string.size();
        fixed.body_size = 0;

        client->Write(View(fixed));
        client->Write(View(header_string));
    };

    RequestHeader header;
    if (!header.ParseFromString(header_string)) {
        reply_error("Header parsing failed");
        return;
    }

    IRpcService* service = nullptr;
    const google::protobuf::MethodDescriptor* method = nullptr;

    try {
        auto it = services.find(header.service());
        if (it == services.end()) {
            throw std::runtime_error{"Service " + header.service() + " not found"};
        }

        service = it->second.get();
        method = service->ServiceDescriptor()->FindMethodByName(header.method());
        if (!method) {
            throw std::runtime_error{"Method " + header.method() + " not found"};
        }
    } catch (const std::exception& ex) {
        reply_error(ex.what());
        return;
    }

    auto factory = google::protobuf::MessageFactory::generated_factory();
    auto request_proto = factory->GetPrototype(method->input_type());
    auto response_proto = factory->GetPrototype(method->output_type());

    std::unique_ptr<google::protobuf::Message> request{request_proto->New()};
    std::unique_ptr<google::protobuf::Message> response{response_proto->New()};

    if (!request->ParseFromString(body_string)) {
        reply_error("Body parsing failed");
        return;
    }

    try {
        service->CallMethod(method, *request, response.get());
    } catch (const std::exception& ex) {
        reply_error(ex.what());
        return;
    }

    body_string = response->SerializeAsString();
    fixed.header_size = 0;
    fixed.body_size = body_string.size();

    client->Write(View(fixed));
    client->Write(View(body_string));
}

}  // namespace

void SimpleRpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                  const google::protobuf::Message& request,
                                  google::protobuf::Message* response) {
    auto conn = DialTCP(server_);

    RequestHeader req_header;
    req_header.set_service(method->service()->full_name());
    req_header.set_method(method->name());

    auto header_string = req_header.SerializeAsString();
    auto body_string = request.SerializeAsString();

    FixedHeader fixed{.header_size = static_cast<uint32_t>(header_string.size()),
                      .body_size = static_cast<uint32_t>(body_string.size())};
    conn->Write(View(fixed));
    conn->Write(View(header_string));
    conn->Write(View(body_string));

    conn->ReadFull(View(fixed));

    header_string.resize(fixed.header_size);
    body_string.resize(fixed.body_size);

    conn->ReadFull(View(header_string));
    conn->ReadFull(View(body_string));

    if (ResponseHeader rsp_header; !rsp_header.ParseFromString(header_string)) {
        throw RpcCallError{"Header parsing failed"};
    } else if (rsp_header.has_rpc_error()) {
        throw RpcCallError{rsp_header.rpc_error().message()};
    } else if (!response->ParseFromString(body_string)) {
        throw RpcCallError{"Body parsing failed"};
    }
}

void SimpleRpcServer::Serve() {
    while (true) {
        std::shared_ptr client = lsn_->Accept();
        group_.Spawn([this, client] { HandleConnection(client.get(), services_); });
    }
}

}  // namespace cactus
