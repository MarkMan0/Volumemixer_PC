cmake_minimum_required(VERSION 3.18.0)


add_library(SerialPortWrapper src/SerialPortWrapper.cpp)
target_include_directories(SerialPortWrapper PUBLIC include/)
