add_shad_library(redis_net epoll.cpp event_loop.cpp)
target_include_directories(redis_net PUBLIC ..)

add_subdirectory(tests)
