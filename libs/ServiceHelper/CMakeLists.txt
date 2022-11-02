cmake_minimum_required (VERSION 3.5)


add_library(ServiceHelper STATIC "src/ServiceUtils.cpp")
target_include_directories(ServiceHelper PUBLIC "include")
