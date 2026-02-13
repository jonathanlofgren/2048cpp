cmake_minimum_required(VERSION 3.14)
project(2048cpp
    VERSION 1.0.0
    DESCRIPTION "Highly optimized AI for 2048 using bitboards and parallelized expectimax"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library (shared between game and tests)
add_library(2048core STATIC
    bitboard.cpp
    search.cpp
)
target_include_directories(2048core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link pthreads for std::async/std::thread
find_package(Threads REQUIRED)
target_link_libraries(2048core PUBLIC Threads::Threads)

# Main game executable
add_executable(2048cpp main.cpp)
target_link_libraries(2048cpp PRIVATE 2048core)

# Tests
enable_testing()

add_executable(2048tests tests/test_main.cpp)
target_link_libraries(2048tests PRIVATE 2048core)

add_test(NAME bitboard_conversion COMMAND 2048tests bitboard_conversion)
add_test(NAME move_left_right COMMAND 2048tests move_left_right)
add_test(NAME move_consistency COMMAND 2048tests move_consistency)
add_test(NAME empty_squares COMMAND 2048tests empty_squares)
add_test(NAME place_random COMMAND 2048tests place_random)
add_test(NAME evaluation COMMAND 2048tests evaluation)
