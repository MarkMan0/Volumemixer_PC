cmake_minimum_required(VERSION 3.23.0)


add_executable(volumeAPI_CLI "main.cpp")
target_link_libraries(volumeAPI_CLI VolumeAPI)
