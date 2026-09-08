cmake_minimum_required(VERSION 3.20)
project(market_data_parser)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(SPDLOG_USE_STD_FORMAT)

option(ENABLE_NSE_L2_LZO "Build NSE L2 parser with miniLZO (GPL-2+ combined work)" OFF)
option(ENABLE_SANITIZERS "AddressSanitizer + UndefinedBehaviorSanitizer (unit tests)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "ENABLE_SANITIZERS is not supported with MSVC")
    endif()
    add_compile_options(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    add_link_options(-fsanitize=address,undefined)
elseif(MSVC)
    add_compile_options(/O2 /arch:AVX2)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic)
    if(APPLE)
        add_compile_options(-mcpu=apple-m1)
    else()
        add_compile_options(-march=native)
    endif()
    add_compile_options(-flto)
    add_link_options(-flto)
endif()

include_directories(include)

include(FetchContent)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library." FORCE)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(googlebenchmark)


FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

add_library(market_data_lib STATIC
    src/parsers/nse/NseFoParser.cpp
)
target_include_directories(market_data_lib PUBLIC include)
target_link_libraries(market_data_lib PUBLIC spdlog::spdlog_header_only tomlplusplus::tomlplusplus)

if(ENABLE_NSE_L2_LZO)
    message(STATUS "NSE L2 + miniLZO enabled: resulting binaries that link market_data_lib are GPL-2+ combined works")
    target_sources(market_data_lib PRIVATE
        src/parsers/nse/NseL2Parser.cpp
        src/minilzo/minilzo.c
    )
    target_compile_definitions(market_data_lib PUBLIC MARKET_DATA_HAS_NSEL2_LZO)
else()
    message(STATUS "NSE L2 + miniLZO disabled (default). Reconfigure with -DENABLE_NSE_L2_LZO=ON to opt in to GPL-2+")
endif()


add_executable(market_data_parser src/main.cpp src/Config.cpp)
target_link_libraries(market_data_parser PRIVATE market_data_lib tomlplusplus::tomlplusplus)

add_executable(benchmark_runner benchmarks/benchmark.cpp)
target_link_libraries(benchmark_runner PRIVATE market_data_lib benchmark::benchmark)

enable_testing()
add_executable(unit_tests 
    tests/OrderBookTest.cpp
    tests/NseParserTest.cpp
    tests/ItchParserTest.cpp
    tests/DenseMapTest.cpp
)
target_link_libraries(unit_tests PRIVATE market_data_lib gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)



