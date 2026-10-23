add_shad_library(redis_io async_listener.cpp socket.cpp async_reader.cpp)
target_link_libraries(redis_io PUBLIC cactus redis_net)
target_include_directories(redis_io PUBLIC ..)

add_subdirectory(tests)
