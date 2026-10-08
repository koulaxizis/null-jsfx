# JSFX-vs-native comparison test (Linux/macOS; uses dlopen + ysfx)
# Build ysfx first, e.g. with tests/run.sh at the repo root.

set(YSFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../tests/.build/ysfx" CACHE PATH "ysfx source tree with build/libysfx.a")

add_executable(compare_tilt compare_tilt.cpp)
target_include_directories(compare_tilt PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../plugin"
  "${YSFX_ROOT}/include"
  "${dpf_SOURCE_DIR}/distrho/src")
target_link_libraries(compare_tilt PRIVATE "${YSFX_ROOT}/build/libysfx.a" ${CMAKE_DL_LIBS} pthread)

set(_jsfx "${CMAKE_CURRENT_SOURCE_DIR}/../../../../DATA/Effects/null_jsfx/tilt.jsfx")
if(APPLE)
  # ysfx is built for the host arch only; the plugin itself stays universal.
  set_target_properties(compare_tilt PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
  set(_clap_bin "${CMAKE_BINARY_DIR}/bin/null_tilt_native.clap/Contents/MacOS/null_tilt_native")
else()
  set(_clap_bin "${CMAKE_BINARY_DIR}/bin/null_tilt_native.clap")
endif()

if(TARGET null_tilt_native-clap)
  add_test(NAME compare_tilt COMMAND compare_tilt "${_jsfx}" "${_clap_bin}")
else()
  add_test(NAME compare_tilt COMMAND compare_tilt "${_jsfx}")
endif()
