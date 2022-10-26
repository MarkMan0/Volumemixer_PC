

add_library(ComEnum "src/ComEnum.cpp")
target_include_directories(ComEnum PUBLIC "include")
target_compile_definitions(ComEnum PUBLIC _UNICODE)
