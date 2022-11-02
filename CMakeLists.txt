cmake_minimum_required(VERSION 3.23.0)



project(MixerClient)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_compile_definitions("UNICODE" "_UNICODE")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options("/permissive-")
endif()

add_subdirectory("libs/")
add_subdirectory("examples/")
add_subdirectory("MixerClient")
