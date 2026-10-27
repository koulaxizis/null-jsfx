# JSFX-vs-native comparison tests (Linux/macOS; uses dlopen + ysfx). Build ysfx first,
# e.g. with tests/run.sh at the repo root.
#
# Per plugin: compare_<name> runs DATA/Effects/null_jsfx/<name>.jsfx against the CLAP build,
# presets_<name> does the same with each factory preset (<name>.jsfx.rpl), gfx_<name> runs it
# against DATA/Effects/null_jsfx/gfx/<name>_gfx.jsfx, state_<name> checks its state round trip
# (state_test.cpp, needs no ysfx). A plugin can pass
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

# state_test: state_<name> saves random parameter values and loads them back through streams that
# move a few bytes per call (like clap-validator's state-reproducibility-buffered)
add_executable(state_test state_test.cpp)
target_include_directories(state_test PRIVATE "${dpf_SOURCE_DIR}/distrho/src")
target_link_libraries(state_test PRIVATE ${CMAKE_DL_LIBS})
if(APPLE)
  set_target_properties(state_test PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

# module_compare: compare.cpp with the Rack's modules linked in; module_<id> checks each module
# against its JSFX like compare_<id> checks the plugin
if(NULL_RACK_MODULES)
  add_executable(module_compare compare.cpp)
  target_compile_definitions(module_compare PRIVATE NULL_MODULE_TEST=1)
  target_include_directories(module_compare PRIVATE "${YSFX_ROOT}/include" "${dpf_SOURCE_DIR}/distrho/src"
    "${dpf_SOURCE_DIR}/distrho" "${CMAKE_CURRENT_SOURCE_DIR}/module_info" "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_BINARY_DIR}/generated")
  target_link_libraries(module_compare PRIVATE "${YSFX_ROOT}/build/libysfx.a" ${CMAKE_DL_LIBS} pthread)
  if(APPLE)
    set_target_properties(module_compare PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
  endif()
  foreach(_id ${NULL_RACK_MODULES})
    set(_args)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/${_id}/compare_args.txt")
      file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/${_id}/compare_args.txt" _a)
      string(STRIP "${_a}" _a)
      separate_arguments(_args UNIX_COMMAND "${_a}")
    endif()
    add_test(NAME module_${_id} COMMAND module_compare "${_jsfx_dir}/${_id}.jsfx" "module:${_id}" ${_args})
  endforeach()

  # rack_test: the Rack's engine (plugins/rack/RackEngine.hpp) with the same modules: presets,
  # bit-exactness, bands, channels, modulation. A build limited by NULL_MODULES skips the presets
  # whose modules it lacks; a full build fails on them.
  add_executable(rack_test "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/rack/test/rack_test.cpp")
  target_include_directories(rack_test PRIVATE "${dpf_SOURCE_DIR}/distrho/src" "${dpf_SOURCE_DIR}/distrho"
    "${CMAKE_CURRENT_SOURCE_DIR}/module_info" "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/rack" "${CMAKE_BINARY_DIR}/generated")
  target_link_libraries(rack_test PRIVATE pthread)
  if(NOT NULL_MODULES)
    target_compile_definitions(rack_test PRIVATE RACK_FULL_BUILD=1)
  endif()
  if(APPLE)
    set_target_properties(rack_test PROPERTIES OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
  endif()
  add_test(NAME rack_test COMMAND rack_test)
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
    # the factory presets (tools/build_presets.py): every preset sounds the same in the CLAP
    if(EXISTS "${_jsfx_dir}/${_p}.jsfx.rpl")
      add_test(NAME presets_${_p} COMMAND compare "${_jsfx_dir}/${_p}.jsfx" "${_bin}" ${_args} --rpl "${_jsfx_dir}/${_p}.jsfx.rpl")
    endif()
  endif()
  # the saved state comes back exactly, whatever size the host's stream reads and writes are
  if(TARGET null_jsfx_${_p}-clap)
    add_test(NAME state_${_p} COMMAND state_test "${_bin}")
  endif()
  if(EXISTS "${_jsfx_dir}/gfx/${_p}_gfx.jsfx")
    add_test(NAME gfx_${_p} COMMAND compare "${_jsfx_dir}/${_p}.jsfx" "${_jsfx_dir}/gfx/${_p}_gfx.jsfx" --tol 0)
  endif()
endforeach()
