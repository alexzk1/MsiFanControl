cmake_minimum_required(VERSION 3.15)

project(MsiFanCtrlD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(CPUID cpuid REQUIRED)

set(Boost_USE_MULTITHREADED ON)

find_package(Boost 1.80 COMPONENTS system REQUIRED)
find_package(cereal REQUIRED)

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(FindSeccomp)
find_package(Seccomp REQUIRED)

add_executable(MsiFanCtrlD
 maind.cpp
 communicator.h communicator.cpp
 seccomp_wrapper.hpp
)

target_include_directories(MsiFanCtrlD PRIVATE
                                          ${CMAKE_CURRENT_LIST_DIR}/../common
                                          ${CMAKE_CURRENT_LIST_DIR}/../libMsiFanControl
                                          ${Boost_INCLUDE_DIRS}
                                          ${Seccomp_INCLUDE_DIRS}
)
target_link_libraries(MsiFanCtrlD PRIVATE MsiFanControl
                                          ${CPUID}
                                          systemd pthread
                                          ${Boost_LIBRARIES}
                                          ${cereal_LIBRARIES}
                                          ${Seccomp_LIBRARIES}
)

