

set(CMAKE_MFC_FLAG 1)
add_library(enumser "src/enumser.cpp")
target_include_directories(enumser PUBLIC "include")
target_compile_definitions(enumser PUBLIC _AFXDLL)
