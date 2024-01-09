cmake_minimum_required(VERSION 3.5)

project(MsiFanControlGUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Boost 1.80 COMPONENTS system program_options REQUIRED)
find_package(cereal REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        communicator.cpp communicator.h
        execonmainthread.cpp execonmainthread.h
    )

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MsiFanControlGUI
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resources.qrc
        delayed_buttons.h
        gui_helpers.h
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MsiFanControlGUI APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(MsiFanControlGUI SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MsiFanControlGUI
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(MsiFanControlGUI PRIVATE ../libMsiFanControl)
target_include_directories(MsiFanControlGUI PRIVATE ../common)
target_link_libraries(MsiFanControlGUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets ${Boost_LIBRARIES} ${cereal_LIBRARIES})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MsiFanControlGUI)
endif()
set_target_properties(MsiFanControlGUI PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MsiFanControlGUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MsiFanControlGUI)
endif()
