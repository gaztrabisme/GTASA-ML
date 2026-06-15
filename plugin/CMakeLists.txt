# CMakeLists.txt — OPTIONAL build path for the GTASA-ML bridge plugin (M0).
#
# The lower-risk path is the Plugin SDK's own Visual Studio project template
# (see README.md). This file is a starting point for a CMake build; you will likely
# need to adjust how the Plugin SDK is located/linked to match your SDK checkout.
#
# IMPORTANT: GTA SA is 32-bit. Always configure for Win32:
#   cmake -B build -A Win32 -DPLUGIN_SDK_DIR=C:/path/to/plugin-sdk
#   cmake --build build --config Release

cmake_minimum_required(VERSION 3.20)
project(GTASAML LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
    message(WARNING
        "GTA SA is 32-bit. Configure with -A Win32, e.g.\n"
        "  cmake -B build -A Win32 -DPLUGIN_SDK_DIR=...")
endif()

# Path to your Plugin SDK checkout. Override on the command line with -DPLUGIN_SDK_DIR=...
# Falls back to the PLUGIN_SDK_DIR environment variable if set.
set(PLUGIN_SDK_DIR "$ENV{PLUGIN_SDK_DIR}" CACHE PATH "Path to DK22Pac plugin-sdk checkout")

if(NOT PLUGIN_SDK_DIR)
    message(FATAL_ERROR "Set -DPLUGIN_SDK_DIR=C:/path/to/plugin-sdk (or the env var).")
endif()

# Build the plugin as a DLL, then rename the output to .asi (what the ASI loader scans for).
add_library(GTASAML SHARED
    src/Main.cpp
    src/HeartbeatServer.h
)

set_target_properties(GTASAML PROPERTIES
    SUFFIX ".asi"
    PREFIX ""
)

# --- Plugin SDK include/link ---------------------------------------------------------------
# The exact include and library locations depend on how you built the SDK. The common layout
# exposes a top-level "plugin.h" plus the per-game headers under plugin_sa/. Adjust as needed.
target_include_directories(GTASAML PRIVATE
    "${PLUGIN_SDK_DIR}/shared"
    "${PLUGIN_SDK_DIR}/plugin_sa"
    "${PLUGIN_SDK_DIR}/plugin_sa/game_sa"
)

# The SDK is typically consumed as a prebuilt static lib (e.g. plugin_sa.lib). Point this at
# wherever your build produced it. If you instead add the SDK as a CMake subproject, replace
# this with target_link_libraries(GTASAML PRIVATE plugin_sa).
find_library(PLUGIN_SA_LIB
    NAMES plugin_sa
    HINTS "${PLUGIN_SDK_DIR}/output/lib" "${PLUGIN_SDK_DIR}/lib"
)
if(PLUGIN_SA_LIB)
    target_link_libraries(GTASAML PRIVATE "${PLUGIN_SA_LIB}")
else()
    message(WARNING
        "Could not find plugin_sa lib under PLUGIN_SDK_DIR. Set it manually or use the "
        "VS-template build path in README.md.")
endif()

target_link_libraries(GTASAML PRIVATE Ws2_32)
