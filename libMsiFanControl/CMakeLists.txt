cmake_minimum_required(VERSION 3.14)

project(libMsiFanControl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(CPUID cpuid REQUIRED)

include(CMakePrintHelpers)
cmake_print_variables(CPUID)

add_library(MsiFanControl STATIC
  device_commands.h
  readwrite.h
  readwrite_provider.h
  csysfsprovider.h csysfsprovider.cpp

  device.h device.cpp
  intelgen10.h intelgen10.cpp
  intelbeforegen10.h intelbeforegen10.cpp

  msi_fan_control.h msi_fan_control.cpp
)

target_compile_definitions(MsiFanControl PRIVATE LIBMSIFANCONTROL_LIBRARY)
target_include_directories(MsiFanControl PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../common)
target_link_libraries(MsiFanControl ${CPUID})
