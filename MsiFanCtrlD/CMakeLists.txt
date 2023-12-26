cmake_minimum_required(VERSION 3.14)

project(MsiFanCtrlD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(CPUID cpuid REQUIRED)

set(Boost_USE_MULTITHREADED ON)

find_package(Boost 1.80 COMPONENTS system REQUIRED)
find_package(cereal REQUIRED)

add_executable(MsiFanCtrlD
 maind.cpp
 communicator.h communicator.cpp
)

include_directories(${Boost_INCLUDE_DIRS})
target_include_directories(MsiFanCtrlD PRIVATE ../libMsiFanControl)
target_include_directories(MsiFanCtrlD PRIVATE ../common)
target_link_libraries(MsiFanCtrlD PRIVATE MsiFanControl ${CPUID} systemd pthread ${Boost_LIBRARIES} ${cereal_LIBRARIES})

