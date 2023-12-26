# Set the minimum required CMake version:
cmake_minimum_required( VERSION 3.14 FATAL_ERROR )

project(MsiFanControlEverything)

add_subdirectory(libMsiFanControl)
add_subdirectory(MsiFanCtrlD)
add_subdirectory(MsiFanControlGUI)
