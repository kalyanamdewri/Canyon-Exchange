cmake_minimum_required(VERSION 3.20)
project(CanyonExchange VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CANYON_ENABLE_LTO "Enable inter-procedural optimization" ON)

if(CANYON_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

add_library(canyon_engine
  src/matching_engine.cpp
)

target_include_directories(canyon_engine PUBLIC include)
target_compile_options(canyon_engine PRIVATE -O3 -march=native -DNDEBUG)

add_executable(canyon_benchmark src/main.cpp)
target_link_libraries(canyon_benchmark PRIVATE canyon_engine)
target_compile_options(canyon_benchmark PRIVATE -O3 -march=native -DNDEBUG)

add_executable(canyon_tests tests/engine_tests.cpp)
target_link_libraries(canyon_tests PRIVATE canyon_engine)
add_test(NAME canyon_tests COMMAND canyon_tests)

enable_testing()
