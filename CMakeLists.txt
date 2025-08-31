cmake_minimum_required(VERSION 3.31.0)
project(orderbook)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")

add_subdirectory(./include/abseil-cpp)

add_executable(ob_base
    src/main.cpp
    src/orderbook.cpp
    src/edit.cpp
)

add_executable(ob_bench
    src/bench.cpp
    src/orderbook.cpp
    src/edit.cpp
)

target_include_directories(ob_base PRIVATE  ./include/abseil-cpp)
target_include_directories(ob_bench PRIVATE ./include/abseil-cpp)
