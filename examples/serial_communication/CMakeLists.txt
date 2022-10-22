cmake_minimum_required(VERSION 3.23.0)


add_executable(SerialComm main.cpp)
target_link_libraries(SerialComm PUBLIC VolumeAPI SerialPortWrapper)