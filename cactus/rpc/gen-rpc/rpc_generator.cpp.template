#include "rpc_generator.h"

#include <ranges>
#include <memory>
#include <map>
#include <stdexcept>
#include <ranges>

namespace {

template <class T>
auto ToUniquePtr(T* p) {
    return std::unique_ptr<T>{p};
}

}  // namespace

bool RpcGenerator::Generate(const google::protobuf::FileDescriptor* file, const std::string&,
                            google::protobuf::compiler::GeneratorContext* generator_context,
                            std::string*) const {
    auto name = file->name();
    if (!name.ends_with(".proto")) {
        throw std::runtime_error{"Bad file extention"};
    }
    name.resize(name.size() - 6);

    auto out_h_includes = ToUniquePtr(generator_context->OpenForInsert(name + ".pb.h", "includes"));
    google::protobuf::io::Printer pb_includes{out_h_includes.get(), '$'};
    pb_includes.Print("#include <cactus/rpc/rpc.h>\n");
    pb_includes.Print("#include <memory>\n");

    auto out_h = ToUniquePtr(generator_context->OpenForInsert(name + ".pb.h", "namespace_scope"));
    google::protobuf::io::Printer pb_h{out_h.get(), '$'};

    for (auto i : std::views::iota(0, file->service_count())) {
        const auto* service = file->service(i);

        std::map<std::string, std::string> vars{{"class_name", service->name() + "Client"}};

        pb_h.Print(vars, "class $class_name$ {\n");
        pb_h.Print("public:\n");
        pb_h.Indent();
        pb_h.Print(vars, "explicit $class_name$(std::unique_ptr<cactus::IRpcChannel> channel)\n");
        pb_h.Print(vars, "    : channel_{std::move(channel)} {}\n");

        // YOUR CODE GOES HERE...

        pb_h.Outdent();
        pb_h.Print("};\n");
    }

    auto out_cc = ToUniquePtr(generator_context->OpenForInsert(name + ".pb.cc", "namespace_scope"));
    google::protobuf::io::Printer pb_cc{out_cc.get(), '$'};

    // ... AND HERE

    return true;
}
