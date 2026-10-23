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
    auto out_cc = ToUniquePtr(generator_context->OpenForInsert(name + ".pb.cc", "namespace_scope"));
    google::protobuf::io::Printer pb_cc{out_cc.get(), '$'};

    for (auto i : std::views::iota(0, file->service_count())) {
        const auto* service = file->service(i);

        std::map<std::string, std::string> vars{
            {"service_name", service->name()},
            {"class_name", service->name() + "Client"},
        };

        pb_h.Print(vars, "class $class_name$ {\n");
        pb_h.Print("public:\n");
        pb_h.Indent();
        pb_h.Print(vars, "explicit $class_name$(std::unique_ptr<cactus::IRpcChannel> channel)\n");
        pb_h.Print(vars, "    : channel_{std::move(channel)} {}\n");

        for (auto i = 0; i < service->method_count(); ++i) {
            auto* method = service->method(i);
            vars["method_name"] = method->name();
            vars["req_type"] = method->input_type()->name();
            vars["rsp_type"] = method->output_type()->name();
            pb_h.Print(vars, "$rsp_type$ $method_name$(const $req_type$& req);\n");
            pb_cc.Print(vars, "$rsp_type$ $class_name$::");
            pb_cc.Print(vars, "$method_name$(const $req_type$& req) {\n");
            pb_cc.Indent();
            pb_cc.Print(vars, "$rsp_type$ rsp;\n");
            pb_cc.Print(vars, "const auto* descriptor = $service_name$::descriptor();\n");
            pb_cc.Print(vars,
                        "const auto* method = descriptor->FindMethodByName(\"$method_name$\");\n");
            pb_cc.Print(vars, "channel_->CallMethod(method, req, &rsp);\n");
            pb_cc.Print("return rsp;\n");
            pb_cc.Outdent();
            pb_cc.Print("}\n\n");
        }

        pb_h.Outdent();
        pb_h.Print("private:\n");
        pb_h.Indent();
        pb_h.Print("std::unique_ptr<cactus::IRpcChannel> channel_;\n");
        pb_h.Outdent();
        pb_h.Print("};\n\n");

        vars["class_name"] = service->name() + "Handler";
        pb_h.Print(vars, "class $class_name$ : public cactus::IRpcService {\n");
        pb_h.Print("public:\n");
        pb_h.Indent();

        pb_h.Print("void CallMethod(const google::protobuf::MethodDescriptor* method,\n");
        pb_h.Print("                const google::protobuf::Message& req,\n");
        pb_h.Print("                google::protobuf::Message* rsp) override;\n");

        pb_cc.Print(vars, "void $class_name$::CallMethod");
        pb_cc.Print(vars, "(const google::protobuf::MethodDescriptor* method,");
        pb_cc.Print(" const google::protobuf::Message& req,\n");
        pb_cc.Print(" google::protobuf::Message* rsp) {\n");
        pb_cc.Indent();
        pb_cc.Print("switch (method->index()) {");
        pb_cc.Indent();
        for (auto i = 0; i < service->method_count(); ++i) {
            auto* method = service->method(i);
            vars["method_name"] = method->name();
            vars["req_type"] = method->input_type()->name();
            vars["rsp_type"] = method->output_type()->name();
            vars["index"] = std::to_string(i);
            pb_h.Print(vars, "virtual void $method_name$");
            pb_h.Print(vars, "(const $req_type$& req, $rsp_type$* rsp) = 0;\n");
            pb_cc.Print(vars, "\ncase $index$:\n");
            pb_cc.Indent();
            pb_cc.Print(vars, "$method_name$(static_cast<const $req_type$&>(req), ");
            pb_cc.Print(vars, "static_cast<$rsp_type$*>(rsp));\nbreak;");
            pb_cc.Outdent();
        }
        pb_cc.Outdent();
        pb_cc.Print("\n}\n");
        pb_cc.Outdent();
        pb_cc.Print("}\n\n");

        pb_h.Print("const google::protobuf::ServiceDescriptor* ServiceDescriptor() override;\n");
        pb_h.Outdent();
        pb_h.Print("};\n\n");
        pb_cc.Print(
            vars,
            "const google::protobuf::ServiceDescriptor* $class_name$::ServiceDescriptor() {\n");
        pb_cc.Indent();
        pb_cc.Print(vars, "return $service_name$::descriptor();\n");
        pb_cc.Outdent();
        pb_cc.Print("}\n\n");
    }
    return true;
}
