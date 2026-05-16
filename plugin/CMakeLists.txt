cmake_minimum_required(VERSION 3.20)
project(harmonic-kate-plugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)
find_package(Qt6Keychain CONFIG QUIET)
if(NOT Qt6Keychain_FOUND)
    find_package(Qt6Keychain REQUIRED)
endif()
find_package(KF6 REQUIRED COMPONENTS TextEditor I18n XmlGui Config)

# Find Qt6Keychain include directories
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(Qt6Keychain_PKG QUIET Qt6Keychain)
endif()

if(NOT Qt6Keychain_INCLUDE_DIRS AND Qt6Keychain_PKG_INCLUDE_DIRS)
    set(Qt6Keychain_INCLUDE_DIRS ${Qt6Keychain_PKG_INCLUDE_DIRS})
endif()

# Fallback: search common locations
if(NOT Qt6Keychain_INCLUDE_DIRS)
    find_path(Qt6Keychain_INCLUDE_DIRS NAMES keychain.h
        PATHS /usr/include /usr/local/include /usr/include/qt6keychain
        PATH_SUFFIXES qt6keychain)
endif()

# Build the Rust core library
find_program(CARGO cargo REQUIRED)

set(HARMONIC_LIB_DIR "${CMAKE_SOURCE_DIR}/../target/release")
set(HARMONIC_LIB "${HARMONIC_LIB_DIR}/libharmonic.so")

add_custom_command(
    OUTPUT ${HARMONIC_LIB}
    COMMAND ${CARGO} build --release --manifest-path ${CMAKE_SOURCE_DIR}/../Cargo.toml
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
    COMMENT "Building Harmonic Rust core library"
)

add_custom_target(harmonic_rust_lib DEPENDS ${HARMONIC_LIB})

# Kate plugin shared library
add_library(harmonicplugin MODULE
    harmonicplugin.cpp
    harmonicview.cpp
    harmonicconfigpage.cpp
    harmonicchatwidget.cpp
    harmonicacp.cpp
    harmonicmarkdown.cpp
)

add_dependencies(harmonicplugin harmonic_rust_lib)

target_include_directories(harmonicplugin PRIVATE
    ${CMAKE_SOURCE_DIR}/../include
    ${Qt6Keychain_INCLUDE_DIRS}
)

# Set rpath so the plugin finds libharmonic.so relative to its location
set_target_properties(harmonicplugin PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../.."
    BUILD_RPATH "${HARMONIC_LIB_DIR}"
)

target_link_libraries(harmonicplugin
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
    KF6::TextEditor
    KF6::I18n
    KF6::XmlGui
    KF6::ConfigCore
    KF6::ConfigGui
    qt6keychain
    ${HARMONIC_LIB}
)

install(TARGETS harmonicplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/ktexteditor)
install(FILES harmonicplugin.json DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/ktexteditor)
install(FILES harmonicplugin.rc DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/harmonicplugin)
