add_shad_library(query_builder query_builder.cpp)
target_include_directories(query_builder PUBLIC ..)

add_catch(test_query_builder test.cpp)
target_link_libraries(test_query_builder PRIVATE query_builder)
