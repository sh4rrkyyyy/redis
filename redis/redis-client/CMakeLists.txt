add_shad_library(redis_client client.cpp)
target_link_libraries(redis_client PUBLIC query_builder query_parser redis_io redis_net)
target_include_directories(redis_client PUBLIC ..)

add_catch(test_redis_client test.cpp)
target_link_libraries(test_redis_client PRIVATE redis_client)
