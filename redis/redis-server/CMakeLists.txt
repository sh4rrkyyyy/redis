add_shad_library(redis_server server.cpp simple_storage.cpp)
target_link_libraries(redis_server PUBLIC cactus query_builder query_parser redis_io redis_net)
target_include_directories(redis_server PUBLIC ..)
target_compile_options(redis_server PUBLIC -Wno-unused-local-typedefs)

add_catch(test_redis_server test.cpp)
target_link_libraries(test_redis_server PRIVATE redis_server)
