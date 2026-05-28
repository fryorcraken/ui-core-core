cmake_minimum_required(VERSION 3.14)
project(AppUiPlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

logos_module(
    NAME app_ui
    REP_FILE src/app_ui.rep
    SOURCES
        src/app_ui_interface.h
        src/app_ui_plugin.h
        src/app_ui_plugin.cpp
)
