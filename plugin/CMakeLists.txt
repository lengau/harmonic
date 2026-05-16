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
find_package(KF6 REQUIRED COMPONENTS TextEditor I18n XmlGui Config)

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
    ${HARMONIC_LIB}
)

install(TARGETS harmonicplugin DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/ktexteditor)
install(FILES harmonicplugin.json DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/ktexteditor)
install(FILES harmonicplugin.rc DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/harmonicplugin)
