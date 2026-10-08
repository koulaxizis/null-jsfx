# NULL Tilt YS — a single-purpose VST3/CLAP(/AU) plugin that runs one embedded
# JSFX through ysfx (https://github.com/JoepVanlier/ysfx, Apache-2.0).
#
# The upstream ysfx plugin sources are fetched at a pinned commit and patched
# (patches/ysfx-embedded-jsfx.patch); the plugin target itself is defined here,
# so none of ysfx's own plugin targets / IDs are built.

cmake_minimum_required(VERSION 3.22)

# ---- product identity (override per plugin) ---------------------------------
set(NULL_PLUGIN_NAME        "NULL Tilt YS"            CACHE STRING "Plugin name shown in hosts")
set(NULL_PLUGIN_TARGET      "NullTiltYS"              CACHE STRING "CMake target / bundle base name")
set(NULL_PLUGIN_CODE        "NtYs"                    CACHE STRING "4-char plugin code (VST3/AU id)")
set(NULL_MANUFACTURER_CODE  "NulJ"                    CACHE STRING "4-char manufacturer code")
set(NULL_CLAP_ID            "tech.nulljsfx.tilt-ys"   CACHE STRING "Reverse-DNS CLAP id")
set(NULL_BUNDLE_ID          "tech.nulljsfx.tilt-ys"   CACHE STRING "macOS bundle id")
set(NULL_VENDOR             "NULL JSFX"               CACHE STRING "Vendor / manufacturer name")
set(NULL_COMPANY            "Christos Koulaxizis"     CACHE STRING "Company (copyright) name")
set(NULL_URL                "https://nulljsfx.tech"   CACHE STRING "Vendor URL")
set(NULL_VERSION            "2.0.0"                   CACHE STRING "Plugin version")
set(NULL_JSFX_FILE "${CMAKE_CURRENT_LIST_DIR}/../gfx/null_tilt_gfx.jsfx" CACHE FILEPATH "JSFX file embedded in the binary")

project(NullTiltYS VERSION ${NULL_VERSION} LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Static MSVC runtime for everything (ysfx, JUCE, plugin): no VC++ redist needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

get_filename_component(NULL_JSFX_FILE "${NULL_JSFX_FILE}" ABSOLUTE)
if(NOT EXISTS "${NULL_JSFX_FILE}")
    message(FATAL_ERROR "NULL_JSFX_FILE does not exist: ${NULL_JSFX_FILE}\n"
                        "Pass -DNULL_JSFX_FILE=/path/to/effect.jsfx")
endif()
message(STATUS "Embedding JSFX: ${NULL_JSFX_FILE}")

include(FetchContent)

# ---- ysfx (pinned) -----------------------------------------------------------
set(NULL_YSFX_REPO "https://github.com/JoepVanlier/ysfx.git" CACHE STRING "")
set(NULL_YSFX_SHA  "5c3452fee62583aa3d1b7e877d0c758c4024af89" CACHE STRING "") # master 2026-08-19, ysfx 0.0.47
FetchContent_Declare(ysfx
    GIT_REPOSITORY "${NULL_YSFX_REPO}"
    GIT_TAG        "${NULL_YSFX_SHA}"
    GIT_SUBMODULES "thirdparty/dr_libs" "thirdparty/clap-juce-extensions"
    GIT_SUBMODULES_RECURSE TRUE
    PATCH_COMMAND  "${CMAKE_COMMAND}"
                   "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/patches/ysfx-embedded-jsfx.patch"
                   -P "${CMAKE_CURRENT_LIST_DIR}/cmake/apply_patch.cmake"
    UPDATE_DISCONNECTED TRUE)

# Only the ysfx library is built from upstream; the plugin target is ours.
set(YSFX_PLUGIN OFF CACHE BOOL "" FORCE)
set(YSFX_TESTS OFF CACHE BOOL "" FORCE)
set(YSFX_TOOLS OFF CACHE BOOL "" FORCE)
set(YSFX_GFX ON CACHE BOOL "" FORCE)
set(YSFX_SKIP_CHECKSUM ON CACHE BOOL "" FORCE)   # upstream uses CMAKE_SOURCE_DIR there
FetchContent_MakeAvailable(ysfx)

# ---- JUCE (same version upstream ysfx uses) ----------------------------------
# (git instead of the release tarball: archive downloads are blocked on some
#  CI/proxy setups. Use -DFETCHCONTENT_SOURCE_DIR_JUCE=/path to reuse a checkout.)
FetchContent_Declare(juce
    GIT_REPOSITORY "https://github.com/juce-framework/JUCE.git"
    GIT_TAG        "8.0.6"
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(juce)

# clap-juce-extensions comes pinned as a submodule of the ysfx commit above.
add_subdirectory("${ysfx_SOURCE_DIR}/thirdparty/clap-juce-extensions"
                 "${CMAKE_CURRENT_BINARY_DIR}/clap-juce-extensions" EXCLUDE_FROM_ALL)

# ---- embed the JSFX ----------------------------------------------------------
# Number of host-visible parameters = highest sliderN: index in the JSFX.
file(STRINGS "${NULL_JSFX_FILE}" _slider_lines REGEX "^[ \t]*slider[0-9]+:")
set(NULL_NUM_PARAMS 0)
foreach(_l IN LISTS _slider_lines)
    string(REGEX MATCH "slider([0-9]+):" _m "${_l}")
    if(CMAKE_MATCH_1 GREATER NULL_NUM_PARAMS)
        set(NULL_NUM_PARAMS ${CMAKE_MATCH_1})
    endif()
endforeach()
message(STATUS "JSFX sliders exposed as parameters: ${NULL_NUM_PARAMS}")

get_filename_component(NULL_JSFX_NAME "${NULL_JSFX_FILE}" NAME)
set(_embedded_cpp "${CMAKE_CURRENT_BINARY_DIR}/null_embedded_data.cpp")
add_custom_command(
    OUTPUT "${_embedded_cpp}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${NULL_JSFX_FILE}" "-DOUTPUT=${_embedded_cpp}"
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/embed_file.cmake"
    DEPENDS "${NULL_JSFX_FILE}" "${CMAKE_CURRENT_LIST_DIR}/cmake/embed_file.cmake"
    COMMENT "Embedding ${NULL_JSFX_NAME}")

# ---- plugin ------------------------------------------------------------------
if(APPLE)
    set(_formats VST3 AU)
else()
    set(_formats VST3)
endif()

juce_add_plugin(${NULL_PLUGIN_TARGET}
    PRODUCT_NAME "${NULL_PLUGIN_NAME}"
    PLUGIN_NAME "${NULL_PLUGIN_NAME}"
    DESCRIPTION "${NULL_PLUGIN_NAME} by ${NULL_VENDOR}"
    VERSION "${NULL_VERSION}"
    COMPANY_NAME "${NULL_VENDOR}"
    COMPANY_COPYRIGHT "${NULL_COMPANY}"
    COMPANY_WEBSITE "${NULL_URL}"
    BUNDLE_ID "${NULL_BUNDLE_ID}"
    PLUGIN_CODE "${NULL_PLUGIN_CODE}"
    PLUGIN_MANUFACTURER_CODE "${NULL_MANUFACTURER_CODE}"
    FORMATS ${_formats}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(_ys "${ysfx_SOURCE_DIR}/plugin")
target_sources(${NULL_PLUGIN_TARGET} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/null_embedded.cpp"
    "${_embedded_cpp}"
    "${_ys}/processor.cpp"
    "${_ys}/editor.cpp"
    "${_ys}/lookandfeel.cpp"
    "${_ys}/parameter.cpp"
    "${_ys}/info.cpp"
    "${_ys}/bank_io.cpp"
    "${_ys}/components/parameters_panel.cpp"
    "${_ys}/components/graphics_view.cpp"
    "${_ys}/components/ide_view.cpp"
    "${_ys}/components/rpl_view.cpp"
    "${_ys}/components/dialogs.cpp"
    "${_ys}/components/tokenizer.cpp"
    "${_ys}/components/ysfx_document.cpp"
    "${_ys}/components/misc_buttons.cpp"
    "${_ys}/utility/async_updater.cpp"
    "${_ys}/utility/rt_semaphore.cpp")

target_include_directories(${NULL_PLUGIN_TARGET} PRIVATE "${_ys}" "${CMAKE_CURRENT_LIST_DIR}/src")

target_compile_definitions(${NULL_PLUGIN_TARGET} PUBLIC
    VERSION_STRING="${NULL_VERSION}"
    NULL_EMBEDDED_JSFX=1
    NULL_NUM_PARAMS=${NULL_NUM_PARAMS}
    NULL_JSFX_NAME="${NULL_JSFX_NAME}"
    NULL_CACHE_VENDOR="${NULL_VENDOR}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(${NULL_PLUGIN_TARGET} PRIVATE
    ysfx::ysfx
    json
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(
    TARGET ${NULL_PLUGIN_TARGET}
    CLAP_ID "${NULL_CLAP_ID}"
    CLAP_FEATURES audio-effect equalizer stereo
    CLAP_MANUAL_URL "${NULL_URL}"
    CLAP_SUPPORT_URL "${NULL_URL}"
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
    CLAP_USE_JUCE_PARAMETER_RANGES DISCRETE)

# ---- optional headless CLAP render host (used by test/compare.sh) -------------
option(NULL_BUILD_TESTS "Build the headless CLAP render test host" OFF)
if(NULL_BUILD_TESTS AND NOT WIN32)
    add_executable(clap_render test/clap_render.cpp)
    target_include_directories(clap_render PRIVATE
        "${ysfx_SOURCE_DIR}/thirdparty/clap-juce-extensions/clap-libs/clap/include")
    target_link_libraries(clap_render PRIVATE ${CMAKE_DL_LIBS})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(X11 REQUIRED)
        add_executable(clap_gui test/clap_gui.cpp)
        target_include_directories(clap_gui PRIVATE
            "${ysfx_SOURCE_DIR}/thirdparty/clap-juce-extensions/clap-libs/clap/include")
        target_link_libraries(clap_gui PRIVATE X11::X11 ${CMAKE_DL_LIBS})
    endif()
endif()

# Keep the statically linked ysfx/EEL2/SWELL symbols private (only the plugin
# entry points are exported), so several ysfx-based plugins can coexist in one
# host process on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(_fmt VST3 CLAP)
        if(TARGET ${NULL_PLUGIN_TARGET}_${_fmt})
            target_link_options(${NULL_PLUGIN_TARGET}_${_fmt} PRIVATE "LINKER:--exclude-libs,ALL")
        endif()
    endforeach()
endif()
