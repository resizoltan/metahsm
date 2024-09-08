cmake_minimum_required(VERSION 3.19)
project(metahsm)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)


if(BUILD_TESTING)
    add_test(test test.cpp)
    target_include_directories (test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
