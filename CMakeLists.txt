cmake_minimum_required(VERSION 3.19)
project(metahsm)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

include(FetchContent)
include(CTest)

#add_compile_options(-ftime-trace -c)

#FetchContent_Declare(
#  Catch2
#  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#  GIT_TAG        v3.4.0
#)
#FetchContent_MakeAvailable(Catch2)

if(BUILD_TESTING)
    add_executable(tests test.cpp)
    target_include_directories (tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
