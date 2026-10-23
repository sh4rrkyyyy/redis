enable_language(ASM)

set(CONTEXT_ASM "internal/context_x86_64.S")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: " ${CMAKE_HOST_SYSTEM_PROCESSOR})
if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "^(aarch64|arm64)$")
  set(CONTEXT_ASM "internal/context_arm64.S")
endif()

file(GLOB CORE_SRC CONFIGURE_DEPENDS "core/*.cpp")
file(GLOB INT_SRC CONFIGURE_DEPENDS "internal/*.cpp")
file(GLOB IO_SRC CONFIGURE_DEPENDS "io/*.cpp")
file(GLOB NET_SRC CONFIGURE_DEPENDS "net/*.cpp")
file(GLOB SYNC_SRC CONFIGURE_DEPENDS "sync/*.cpp")

add_library(cactus
  ${CORE_SRC}
  ${INT_SRC}
  ${IO_SRC}
  ${NET_SRC}
  ${SYNC_SRC}
  ${CONTEXT_ASM})

find_package(glog REQUIRED)
target_link_libraries(cactus PUBLIC glog::glog)
target_include_directories(cactus PUBLIC ..)

add_subdirectory(examples)
add_subdirectory(rpc)
add_subdirectory(tests)
