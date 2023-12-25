cmake_minimum_required(VERSION 3.14)

project(MsiFanCtrlD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(CPUID cpuid REQUIRED)

add_executable(MsiFanCtrlD
 maind.cpp
 runners.h
)

target_include_directories(MsiFanCtrlD PRIVATE ../libMsiFanControl)
target_link_libraries(MsiFanCtrlD PRIVATE MsiFanControl ${CPUID} systemd pthread)
