# Set the minimum required CMake version:
cmake_minimum_required( VERSION 3.15 FATAL_ERROR )

project(MsiFanControlEverything)

option(BUILD_TESTS "Build tests" ON)

add_subdirectory(libMsiFanControl)
add_subdirectory(MsiFanCtrlD)
add_subdirectory(MsiFanControlGUI)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
