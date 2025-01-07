cmake_minimum_required(VERSION 3.16)

project(Projekt_mp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

file(GLOB MAINWINDOW_FILES
    src/MainWindow/*.cpp
    src/MainWindow/*.h
    src/MainWindow/*.ui
)

file(GLOB MANAGER_FILES
    src/Managers/*.cpp
    src/Managers/*.h
)

file(GLOB THEME_FILES
    src/Themes/*.qss
)

source_group("MainWindow" FILES ${MAINWINDOW_FILES})
source_group("Managers" FILES ${MANAGER_FILES})
source_group("Themes" FILES ${THEME_FILES})

# Combine all files into PROJECT_SOURCES
set(PROJECT_SOURCES
    src/main.cpp
    ${MAINWINDOW_FILES}
    ${MANAGER_FILES}
    ${THEME_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Projekt_mp
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        src/Managers/emailmanager.h src/Managers/emailmanager.cpp
        src/Managers/usermanager.h src/Managers/usermanager.cpp
        src/MainWindow/addaccountdialog.h src/MainWindow/addaccountdialog.cpp src/MainWindow/addaccountdialog.ui
        src/Themes/light.qss src/Themes/dark.qss
        src/Managers/connectionmanager.h src/Managers/connectionmanager.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Projekt_mp APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Projekt_mp SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Projekt_mp
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Projekt_mp PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Projekt_mp)
endif()
set_target_properties(Projekt_mp PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Projekt_mp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Projekt_mp)
endif()
