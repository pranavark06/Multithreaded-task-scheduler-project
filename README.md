cmake_minimum_required(VERSION 3.16)
project(task_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(task_scheduler src/main.cpp)

target_include_directories(task_scheduler PRIVATE ${CMAKE_SOURCE_DIR}/include)
