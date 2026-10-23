Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.13.0
)

set(CATCH_CONFIG_THREAD_SAFE_ASSERTIONS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Catch2)
