function(add_cactus_test TARGET SOURCES)
  add_catch(${TARGET} ${SOURCES})
  target_link_libraries(${TARGET} PRIVATE cactus)
  target_include_directories(${TARGET} PUBLIC .)
endfunction()

add_cactus_test(test_cactus_core test_core.cpp)
add_cactus_test(test_cactus_internal test_internal.cpp)
add_cactus_test(test_cactus_io test_io.cpp)
add_cactus_test(test_cactus_net test_net.cpp)
add_cactus_test(test_cactus_sync test_sync.cpp)

add_subdirectory(bench)
