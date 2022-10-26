cmake_minimum_required(VERSION 3.23.0)


add_executable(MixerClient "main.cpp" "communication.cpp")
target_link_libraries(MixerClient PUBLIC VolumeAPI SerialPortWrapper CommSupervisor ComEnum)
target_compile_definitions(MixerClient PUBLIC NOMINMAX)
