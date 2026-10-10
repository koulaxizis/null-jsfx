# NULL JSFX - VST3/CLAP builds of the plugins (DPF)
# SPDX-License-Identifier: MIT
#
# Every folder in plugins/ is one plugin: DistrhoPluginInfo.h, Plugin.cpp, UI.cpp and any
# headers it needs. It builds as null_jsfx_<folder>.vst3/.clap. -DNULL_PLUGINS="tilt;louder"
# limits the build to some plugins.

cmake_minimum_required(VERSION 3.15...3.31)

# Universal macOS binaries unless the caller chose otherwise (must precede project()).
if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
endif()

project(null_jsfx_native VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(NULL_UI    "Build the NanoVG GUI (needs X11/OpenGL dev headers on Linux)" ON)
option(NULL_VST3  "Build VST3" ON)
option(NULL_CLAP  "Build CLAP" ON)
option(NULL_JACK  "Build standalones (JACK / native audio), handy for UI screenshots" OFF)
option(NULL_TESTS "Build the JSFX-vs-native comparison tests (needs a built ysfx)" OFF)
set(NULL_PLUGINS "" CACHE STRING "Plugins to build (folder names in plugins/); empty = all")

# ---------------------------------------------------------------------------------------------------------------------
# DPF, pinned. Override with -DFETCHCONTENT_SOURCE_DIR_DPF=/path/to/DPF for offline builds.

set(DPF_GIT_SHA 4238e1c7f0351bbe488d79f0899c540543ac7583) # DPF main, 2025-10-23 "Merge branch 'develop'"

include(FetchContent)

if(MSVC)
  # MSVC ships only OpenGL 1.1 headers; NanoVG needs the GL 2.0 types from Khronos (MIT)
  include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/khronos")
endif()
FetchContent_Declare(dpf
  GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
  GIT_TAG        ${DPF_GIT_SHA}
  GIT_SUBMODULES "dgl/src/pugl-upstream"
  GIT_SHALLOW    FALSE)
set(DPF_LIBRARIES OFF CACHE BOOL "" FORCE)
set(DPF_EXAMPLES  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(dpf)

# ---------------------------------------------------------------------------------------------------------------------
# Plugins

set(_targets)
foreach(_t JACK VST3 CLAP)
  if(NULL_${_t})
    string(TOLOWER ${_t} _lt)
    list(APPEND _targets ${_lt})
  endif()
endforeach()

if(NULL_PLUGINS)
  set(_plugins ${NULL_PLUGINS})
else()
  file(GLOB _dirs LIST_DIRECTORIES true RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/plugins" "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*")
  set(_plugins)
  foreach(_d ${_dirs})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/${_d}/Plugin.cpp")
      list(APPEND _plugins ${_d})
    endif()
  endforeach()
  list(SORT _plugins)
endif()

set(NULL_BUILT_PLUGINS "")
foreach(_p ${_plugins})
  set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/plugins/${_p}")
  set(_ui)
  if(NULL_UI AND EXISTS "${_dir}/UI.cpp")
    set(_ui "${_dir}/UI.cpp")
  endif()
  dpf_add_plugin(null_jsfx_${_p}
    TARGETS ${_targets}
    UI_TYPE opengl
    FILES_DSP "${_dir}/Plugin.cpp"
    FILES_UI  ${_ui})
  target_include_directories(null_jsfx_${_p} PUBLIC "${_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/common")
  if(NOT _ui)
    target_compile_definitions(null_jsfx_${_p} PUBLIC NULL_NO_UI=1)
  endif()
  list(APPEND NULL_BUILT_PLUGINS ${_p})
endforeach()

# ---------------------------------------------------------------------------------------------------------------------
# Tests: each plugin against its JSFX (and the GFX JSFX against the plain one)

if(NULL_TESTS AND NOT WIN32)
  enable_testing()
  add_subdirectory(test)
endif()
