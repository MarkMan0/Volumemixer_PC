cmake_minimum_required(VERSION 3.23.0)


add_executable(SerialComm "main.cpp" "communication.cpp")
target_link_libraries(SerialComm PUBLIC VolumeAPI SerialPortWrapper CommSupervisor)
target_compile_definitions(SerialComm PUBLIC NOMINMAX)