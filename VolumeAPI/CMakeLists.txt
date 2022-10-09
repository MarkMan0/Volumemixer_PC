cmake_minimum_required(VERSION 3.23.0)


set(SOURCES 
    "src/VolumeAPI.cpp"
    "src/process_api.cpp")

add_library(VolumeAPI STATIC ${SOURCES})
target_include_directories(VolumeAPI PUBLIC "include/")
target_compile_definitions(VolumeAPI PUBLIC UNICODE)