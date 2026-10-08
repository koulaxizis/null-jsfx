# NULL Tilt Native - DPF build
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15...3.31)

# Universal macOS binaries unless the caller chose otherwise (must precede project()).
if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
endif()

project(null_tilt_native VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(NULLTILT_UI    "Build the NanoVG GUI (needs X11/OpenGL dev headers on Linux)" ON)
option(NULLTILT_VST3  "Build VST3" ON)
option(NULLTILT_CLAP  "Build CLAP" ON)
option(NULLTILT_LV2   "Build LV2" OFF)
option(NULLTILT_VST2  "Build VST2" OFF)
option(NULLTILT_JACK  "Build standalone (JACK / native audio) - handy for UI screenshots" OFF)
option(NULLTILT_TESTS "Build the JSFX-vs-native comparison test (needs a built ysfx)" OFF)

# ---------------------------------------------------------------------------------------------------------------------
# DPF, pinned. Override with -DFETCHCONTENT_SOURCE_DIR_DPF=/path/to/DPF for offline builds.

set(DPF_GIT_SHA 4238e1c7f0351bbe488d79f0899c540543ac7583) # DPF main, 2025-10-23 "Merge branch 'develop'"

include(FetchContent)
FetchContent_Declare(dpf
  GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
  GIT_TAG        ${DPF_GIT_SHA}
  GIT_SUBMODULES "dgl/src/pugl-upstream"
  GIT_SHALLOW    FALSE)
set(DPF_LIBRARIES OFF CACHE BOOL "" FORCE)
set(DPF_EXAMPLES  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(dpf)

# ---------------------------------------------------------------------------------------------------------------------
# Plugin

set(_targets)
if(NULLTILT_JACK)
  list(APPEND _targets jack)
endif()
if(NULLTILT_LV2)
  list(APPEND _targets lv2)
endif()
if(NULLTILT_VST2)
  list(APPEND _targets vst2)
endif()
if(NULLTILT_VST3)
  list(APPEND _targets vst3)
endif()
if(NULLTILT_CLAP)
  list(APPEND _targets clap)
endif()

set(_ui_files)
if(NULLTILT_UI)
  set(_ui_files plugin/UITilt.cpp)
endif()

dpf_add_plugin(null_tilt_native
  TARGETS ${_targets}
  UI_TYPE opengl
  FILES_DSP plugin/PluginTilt.cpp
  FILES_UI  ${_ui_files})

target_include_directories(null_tilt_native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/plugin")

if(NOT NULLTILT_UI)
  # DistrhoPluginInfo.h enables the UI; switch it off for headless builds.
  target_compile_definitions(null_tilt_native PUBLIC NULLTILT_NO_UI=1)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Tests

if(NULLTILT_TESTS AND NOT WIN32)
  enable_testing()
  add_subdirectory(test)
endif()
