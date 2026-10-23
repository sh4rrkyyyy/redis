add_catch(test_socket test_socket.cpp)
target_link_libraries(test_socket PRIVATE redis_io)
target_include_directories(test_socket PUBLIC ..)

add_catch(test_async_listener test_async_listener.cpp)
target_link_libraries(test_async_listener PRIVATE redis_io)
target_include_directories(test_async_listener PUBLIC ..)
