cmake_minimum_required(VERSION 3.16)
project(mb_mochijsnative C)
# ENGINE_INC: directory holding mbjs.h + mbjs-jit.h (no absolute host paths).
set(ENGINE_INC "${ENGINE_INC}" CACHE PATH "engine native headers dir")
if(NOT ENGINE_INC)
  set(ENGINE_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../bcf/mochiberry")
endif()
# MOCHI_INC: directory holding mbjs_runtime.h.
set(MOCHI_INC "${MOCHI_INC}" CACHE PATH "mochinode headers dir")
if(NOT MOCHI_INC)
  set(MOCHI_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../libmochiberry/mochinode")
endif()
# DYNALIB_DIR: directory holding dynalib.c / dynalib_abi.c / dynalib_trampoline.c + dynalib.h.
set(DYNALIB_DIR "${DYNALIB_DIR}" CACHE PATH "dynalib source dir")
if(NOT DYNALIB_DIR)
  set(DYNALIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libmochiberry/libdynalib")
endif()
add_library(mb_mochijsnative SHARED
  src/mochijsnative.c
  "${DYNALIB_DIR}/dynalib.c"
  "${DYNALIB_DIR}/dynalib_abi.c"
  "${DYNALIB_DIR}/dynalib_trampoline.c")
target_include_directories(mb_mochijsnative PRIVATE
  "${ENGINE_INC}" "${MOCHI_INC}" "${DYNALIB_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(mb_mochijsnative PROPERTIES PREFIX "" OUTPUT_NAME "mochijsnative_x86_64"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
