cmake_minimum_required(VERSION 3.14)
project(AppCorePlugin LANGUAGES CXX)

# Include the Logos Module CMake helper
# For nix builds, this is provided via LOGOS_MODULE_BUILDER_ROOT
if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found")
endif()

# Define the module
logos_module(
    NAME app_core
    SOURCES
        src/app_core_interface.h
        src/app_core_plugin.h
        src/app_core_plugin.cpp
)
