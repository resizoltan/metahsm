cmake_minimum_required(VERSION 3.19)
project(metahsm)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

if(BUILD_TESTING)
    add_executable(tests test.cpp)
    target_include_directories (tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
