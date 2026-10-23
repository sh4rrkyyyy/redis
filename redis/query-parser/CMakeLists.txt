add_shad_library(query_parser parser.cpp)
target_include_directories(query_parser PUBLIC ..)

add_catch(test_query_parser test.cpp)
target_link_libraries(test_query_parser PRIVATE query_parser redis_io)
