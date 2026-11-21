cmake_minimum_required(VERSION 3.20)
project(hw2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

add_executable(analyze src/analyze.cpp)
add_executable(strength-reduce src/strength_reduce.cpp)

foreach(tool IN ITEMS analyze strength-reduce)
    target_include_directories(${tool} SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
    target_link_libraries(${tool} PRIVATE clang-cpp LLVM)
endforeach()
