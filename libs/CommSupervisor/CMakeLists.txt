cmake_minimum_required(VERSION 3.23.0)


add_library(CommSupervisor "src/supervisor.cpp")
target_include_directories(CommSupervisor PUBLIC "include/")