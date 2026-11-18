# Minimal ST plugin: the smallest complete example of the ST Shell conventions.
# Copy this folder to start a new plugin (see docs/NEW_PLUGIN.md).
cmake_minimum_required(VERSION 3.24)
project(STExample VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# In a real plugin repo this points at ../st-shell (sibling checkout) or FetchContent; see st-sat/CMakeLists.txt.
set(ST_SHELL_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Path to st-shell")
add_subdirectory("${ST_SHELL_DIR}" "${CMAKE_BINARY_DIR}/_deps/st-shell-build")

st_add_plugin(STExample
    PRODUCT_NAME  "ST Example"
    PLUGIN_CODE   Stex
    VERSION       ${PROJECT_VERSION}
    SOURCES       Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_include_directories(STExample PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Source")
