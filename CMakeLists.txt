cmake_minimum_required(VERSION 3.20)
project(lumen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

option(LUMEN_SANITIZE "Enable ASan + UBSan" OFF)
option(LUMEN_TSAN "Enable ThreadSanitizer" OFF)
option(LUMEN_FUZZ "Enable libFuzzer build" OFF)
option(LUMEN_BUILD_TESTS "Build GoogleTest suite" ON)
option(LUMEN_BUILD_BENCH "Build benchmark harness" ON)

if(LUMEN_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
  add_link_options(-fsanitize=address,undefined)
endif()

if(LUMEN_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -O1 -g)
  add_link_options(-fsanitize=thread)
endif()

file(GLOB_RECURSE LUMEN_CORE_SOURCES CONFIGURE_DEPENDS
  "src/lexer/*.cpp"
  "src/parser/*.cpp"
  "src/eval/*.cpp"
  "src/bytecode/*.cpp"
  "src/repl/*.cpp"
)

if(LUMEN_CORE_SOURCES)
  add_library(lumen_core STATIC ${LUMEN_CORE_SOURCES})
  target_include_directories(lumen_core PUBLIC src)
endif()

add_executable(lumen src/main.cpp)
if(LUMEN_CORE_SOURCES)
  target_link_libraries(lumen PRIVATE lumen_core)
endif()

if(LUMEN_BUILD_BENCH AND EXISTS "${CMAKE_SOURCE_DIR}/bench/run_bench.cpp")
  add_executable(lumen_bench bench/run_bench.cpp)
  if(LUMEN_CORE_SOURCES)
    target_link_libraries(lumen_bench PRIVATE lumen_core)
  endif()
endif()

if(LUMEN_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  file(GLOB UNIT_TEST_SRCS CONFIGURE_DEPENDS tests/unit/*.cpp)
  if(UNIT_TEST_SRCS)
    add_executable(lumen_unit_tests ${UNIT_TEST_SRCS})
    target_link_libraries(lumen_unit_tests PRIVATE lumen_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(lumen_unit_tests)
  endif()

  file(GLOB E2E_TEST_SRCS CONFIGURE_DEPENDS tests/e2e/*.cpp)
  if(E2E_TEST_SRCS)
    add_executable(lumen_e2e_tests ${E2E_TEST_SRCS})
    target_link_libraries(lumen_e2e_tests PRIVATE lumen_core GTest::gtest_main)
    target_compile_definitions(lumen_e2e_tests PRIVATE
      LUMEN_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
      LUMEN_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/e2e/golden"
    )
    gtest_discover_tests(lumen_e2e_tests)
  endif()
endif()

if(LUMEN_FUZZ AND EXISTS "${CMAKE_SOURCE_DIR}/tests/fuzz/parser_fuzz.cpp")
  add_executable(parser_fuzz tests/fuzz/parser_fuzz.cpp)
  if(LUMEN_CORE_SOURCES)
    target_link_libraries(parser_fuzz PRIVATE lumen_core)
  endif()
  target_compile_options(parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
