# JSFX-vs-native comparison tests (Linux/macOS; uses dlopen + ysfx). Build ysfx first,
# e.g. with tests/run.sh at the repo root.
#
# Per plugin: compare_<name> runs DATA/Effects/null_jsfx/<name>.jsfx against the CLAP build,
# gfx_<name> runs it against DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx. A plugin can pass
# extra arguments (e.g. --tol 1e-4) in plugins/<name>/compare_args.txt.

set(YSFX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/.build/ysfx" CACHE PATH "ysfx source tree with build/libysfx.a")
set(_jsfx_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../DATA/Effects/null_jsfx")

add_executable(compare compare.cpp)
target_include_directories(compare PRIVATE "${YSFX_ROOT}/include" "${dpf_SOURCE_DIR}/distrho/src")
target_link_libraries(compare PRIVATE "${YSFX_ROOT}/build/libysfx.a" ${CMAKE_DL_LIBS} pthread)
if(APPLE)
  # ysfx is built for the host arch only; the plugins themselves stay universal.
  set_target_properties(compare PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

foreach(_p ${NULL_BUILT_PLUGINS})
  set(_args)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/${_p}/compare_args.txt")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/${_p}/compare_args.txt" _a)
    string(STRIP "${_a}" _a)
    separate_arguments(_args UNIX_COMMAND "${_a}")
  endif()
  if(APPLE)
    set(_bin "${CMAKE_BINARY_DIR}/bin/null_jsfx_${_p}.clap/Contents/MacOS/null_jsfx_${_p}")
  else()
    set(_bin "${CMAKE_BINARY_DIR}/bin/null_jsfx_${_p}.clap")
  endif()
  if(TARGET null_jsfx_${_p}-clap AND EXISTS "${_jsfx_dir}/${_p}.jsfx")
    add_test(NAME compare_${_p} COMMAND compare "${_jsfx_dir}/${_p}.jsfx" "${_bin}" ${_args})
  endif()
  if(EXISTS "${_jsfx_dir}/gfx/${_p}_gfx.jsfx")
    add_test(NAME gfx_${_p} COMMAND compare "${_jsfx_dir}/${_p}.jsfx" "${_jsfx_dir}/gfx/${_p}_gfx.jsfx" --tol 0)
  endif()
endforeach()
