add_shad_executable(protoc_gen_rpc main.cpp rpc_generator.cpp)
target_link_libraries(protoc_gen_rpc PRIVATE protobuf protoc)
