cmake_minimum_required(VERSION 3.21)
project(CraftPP VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Catch2 3 REQUIRED)

# ---- core library (logic, no GL) ----
add_library(craftpp_core
  src_cpp/core/log.cpp
  src_cpp/core/random.cpp
  src_cpp/core/math_helper.cpp
  src_cpp/core/vec3.cpp
  src_cpp/core/aabb.cpp
  src_cpp/core/nbt.cpp
)
target_include_directories(craftpp_core PUBLIC src_cpp)
target_link_libraries(craftpp_core PUBLIC ZLIB::ZLIB)

# ---- main application (owns the GL context) ----
add_executable(craftpp
  src_cpp/app/main.cpp
)
target_link_libraries(craftpp PRIVATE craftpp_core glfw OpenGL::GL)

# ---- tests ----
include(CTest)
include(Catch)
add_executable(craftpp_tests
  tests/test_sanity.cpp
  tests/test_random.cpp
  tests/test_math.cpp
  tests/test_nbt.cpp
)
target_link_libraries(craftpp_tests PRIVATE craftpp_core Catch2::Catch2WithMain)
catch_discover_tests(craftpp_tests)
