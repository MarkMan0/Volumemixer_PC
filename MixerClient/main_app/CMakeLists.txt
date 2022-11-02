cmake_minimum_required(VERSION 3.23.0)


add_library(MixerClientLib "src/client.cpp" "src/communication.cpp")
target_include_directories(MixerClientLib PUBLIC "include")
target_link_libraries(MixerClientLib PUBLIC VolumeAPI SerialPortWrapper CommSupervisor ComEnum)
target_compile_definitions(MixerClientLib PUBLIC NOMINMAX)