
project(msi_fan_control_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Tests
file(GLOB_RECURSE TESTS_LIST
     ${CMAKE_CURRENT_LIST_DIR}/*.h
     ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    )
list(LENGTH TESTS_LIST TESTS_LIST_FILES_COUNT)
if (TESTS_LIST_FILES_COUNT GREATER 0)
    find_package(GTest REQUIRED)
    source_group("tests" FILES ${TESTS_LIST})
    add_executable(msi_fan_control_tests
                   ${TESTS_LIST}
    )
    target_link_libraries(msi_fan_control_tests PRIVATE
                        gtest
                        gmock
    )
    target_include_directories(msi_fan_control_tests PUBLIC
                        ${CMAKE_CURRENT_LIST_DIR}
                        ${CMAKE_CURRENT_LIST_DIR}/..
                        ${CMAKE_CURRENT_LIST_DIR}/../MsiFanControlGUI
                    )
    add_test(NAME msi_fan_control_tests COMMAND msi_fan_control_tests)
endif()

