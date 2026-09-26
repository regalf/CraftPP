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
find_package(glm REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EPOXY REQUIRED epoxy)

# ---- core library (logic, no GL calls at runtime in tests) ----
add_library(craftpp_core
  src_cpp/core/log.cpp
  src_cpp/core/random.cpp
  src_cpp/core/math_helper.cpp
  src_cpp/core/vec3.cpp
  src_cpp/core/aabb.cpp
  src_cpp/core/nbt.cpp
  src_cpp/world/block.cpp
  src_cpp/world/biome.cpp
  src_cpp/world/chunk.cpp
  src_cpp/world/genlayer.cpp
  src_cpp/world/noise.cpp
  src_cpp/world/chunk_manager.cpp
  src_cpp/world/provider.cpp
  src_cpp/world/region.cpp
  src_cpp/world/mapgen.cpp
  src_cpp/world/fluid.cpp
  src_cpp/world/worldgen.cpp
  src_cpp/world/live.cpp
  src_cpp/world/block_collision.cpp
  src_cpp/world/block_place.cpp
  src_cpp/entity/entity.cpp
  src_cpp/entity/living.cpp
  src_cpp/entity/items.cpp
  src_cpp/entity/player.cpp
  src_cpp/entity/player_sp.cpp
  src_cpp/entity/controller.cpp
  src_cpp/render/mesher.cpp
  src_cpp/render/frustum.cpp
  src_cpp/render/shader.cpp
  src_cpp/render/texture.cpp
  src_cpp/render/stb_impl.cpp
  src_cpp/render/tessellator.cpp
)
target_include_directories(craftpp_core PUBLIC src_cpp third_party ${EPOXY_INCLUDE_DIRS})
target_link_libraries(craftpp_core PUBLIC ZLIB::ZLIB glm::glm ${EPOXY_LIBRARIES})

# ---- main application (owns the GL context) ----
add_executable(craftpp
  src_cpp/app/main.cpp
)
target_link_libraries(craftpp PRIVATE craftpp_core glfw OpenGL::GL)

# ---- M4 throwaway playable demo (wiring only; will be redone) ----
add_executable(craftpp_demo
  src_cpp/app/demo.cpp
)
target_link_libraries(craftpp_demo PRIVATE craftpp_core glfw OpenGL::GL)

# ---- tests ----
include(CTest)
include(Catch)
add_executable(craftpp_tests
  tests/test_sanity.cpp
  tests/test_random.cpp
  tests/test_math.cpp
  tests/test_nbt.cpp
  tests/test_mesher.cpp
  tests/test_genlayer.cpp
  tests/test_terrain.cpp
  tests/test_mapgen.cpp
  tests/test_populate.cpp
  tests/test_collision.cpp
  tests/test_physics.cpp
  tests/test_living.cpp
  tests/test_player.cpp
  tests/test_live.cpp
  tests/test_frustum.cpp
)
target_link_libraries(craftpp_tests PRIVATE craftpp_core Catch2::Catch2WithMain)
catch_discover_tests(craftpp_tests)
