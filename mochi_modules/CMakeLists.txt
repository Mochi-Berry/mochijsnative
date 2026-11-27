cmake_minimum_required(VERSION 3.16)
project(mb_mochijsnative C)
# ENGINE_INC: directory holding mbjs.h + mbjs_runtime.h (no absolute host paths).
set(ENGINE_INC "${ENGINE_INC}" CACHE PATH "engine native headers dir")
if(NOT ENGINE_INC)
  set(ENGINE_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../libmochiberry/native")
endif()
add_library(mb_mochijsnative SHARED src/mochijsnative.c)
target_include_directories(mb_mochijsnative PRIVATE "${ENGINE_INC}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(WIN32 AND NOT "${ENGINE_INC}" STREQUAL "")
  target_link_libraries(mb_mochijsnative PRIVATE )
endif()
set_target_properties(mb_mochijsnative PROPERTIES PREFIX "" OUTPUT_NAME "mochijsnative_x86_64"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
