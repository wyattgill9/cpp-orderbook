cmake_minimum_required(VERSION 3.30.0)
project(orderbook)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g3 -fsanitize=address,undefined")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
include_directories(${CMAKE_SOURCE_DIR}/includes)

add_executable(orderbook src/main.cpp)
