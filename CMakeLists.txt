cmake_minimum_required(VERSION 3.24)

# ST Shell — shared foundation for ST audio plugins.
#
# Consumers add this directory (add_subdirectory or FetchContent) and then call
# st_add_plugin(...) from their own CMakeLists.txt. See README.md.

project(STShell VERSION 0.1.0 LANGUAGES C CXX)

set(ST_SHELL_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
set(ST_JUCE_VERSION "9.0.5" CACHE STRING "JUCE git tag used when fetching")
set(ST_JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout (optional)")

if(NOT TARGET juce::juce_core)
    if(ST_JUCE_DIR STREQUAL "" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../.st-deps/JUCE-${ST_JUCE_VERSION}/CMakeLists.txt")
        set(ST_JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.st-deps/JUCE-${ST_JUCE_VERSION}")
    endif()

    if(NOT ST_JUCE_DIR STREQUAL "")
        message(STATUS "ST Shell: using JUCE at ${ST_JUCE_DIR}")
        add_subdirectory("${ST_JUCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce-build" EXCLUDE_FROM_ALL)
    else()
        message(STATUS "ST Shell: fetching JUCE ${ST_JUCE_VERSION}")
        include(FetchContent)
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${ST_JUCE_VERSION}
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(juce)
    endif()
endif()

# ---------------------------------------------------------------------------
# Shell module + shared binary resources (fonts)
# ---------------------------------------------------------------------------
juce_add_module("${CMAKE_CURRENT_LIST_DIR}/modules/st_shell" ALIAS_NAMESPACE st)

if(NOT TARGET st_shell_resources)
    juce_add_binary_data(st_shell_resources
        HEADER_NAME   STShellBinary.h
        NAMESPACE     STShellBinary
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/resources/fonts/Inter-Regular.ttf"
            "${CMAKE_CURRENT_LIST_DIR}/resources/fonts/Inter-Medium.ttf"
            "${CMAKE_CURRENT_LIST_DIR}/resources/fonts/Inter-SemiBold.ttf"
            "${CMAKE_CURRENT_LIST_DIR}/resources/fonts/Inter-Bold.ttf")
    set_target_properties(st_shell_resources PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/STShell.cmake")

option(ST_SHELL_BUILD_TESTS "Build ST Shell unit tests" OFF)
if(ST_SHELL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
