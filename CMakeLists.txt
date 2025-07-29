cmake_minimum_required(VERSION 3.31.0)
project(orderbook)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wno-c99-designator -Wno-deprecated-declarations")

add_executable(orderbook src/main.cpp)
target_include_directories(orderbook PRIVATE ${CMAKE_SOURCE_DIR}/include)
