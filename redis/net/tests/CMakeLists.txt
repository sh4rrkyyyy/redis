add_catch(test_epoll test_epoll.cpp)
target_link_libraries(test_epoll PRIVATE redis_net)
target_include_directories(test_epoll PUBLIC ..)

add_catch(test_event_loop test_event_loop.cpp)
target_link_libraries(test_event_loop PRIVATE redis_net)
target_include_directories(test_event_loop PUBLIC ..)
