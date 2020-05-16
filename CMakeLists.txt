cmake_minimum_required(VERSION 3.15)
project(factorio-data-browser CXX C)

set(FACTORIOPATH "C:/Users/micha/factorios/bob_angel - Copy/Factorio_0.18.19")
if(UNIX)
    set(FACTORIOPATH "/home/nexela/factorio")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(options INTERFACE)
target_compile_features(options INTERFACE cxx_std_17)
target_compile_definitions(options INTERFACE FACTORIOPATH=${FACTORIOPATH})
target_compile_definitions(options INTERFACE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING=1)
# Possible to add this to the options target_compile_definitions only?
add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0 _NO_DEBUG_HEAP=1)

# Tell png that we are building zlib alongside the project
set(PNG_BUILD_ZLIB
    ${CMAKE_SOURCE_DIR}/vendor/zlib
    CACHE STRING "Build libfoo shared library" FORCE)

set(ZLIB_LIBRARY zlibstatic)
set(PNG_SHARED
    OFF
    CACHE BOOL "blah" FORCE)
set(PNG_TESTS
    OFF
    CACHE BOOL "blah" FORCE)
set(SKIP_INSTALL_EXPORT
    ON
    CACHE BOOL "blah" FORCE)

# I don't know how to set the include_directores before add_subdirectory png
# Otherwise we have to use ZLIB_ROOT but then cmake complains because it calls
# cmake_policy(3.1)
include_directories(${CMAKE_SOURCE_DIR}/vendor/zlib)
# Also need to have this for zconf.h
include_directories(${CMAKE_BINARY_DIR}/vendor/zlib)
# need this so nana can pick it up
include_directories(${CMAKE_SOURCE_DIR}/vendor/png)
# pnglibconf.h
include_directories(${CMAKE_BINARY_DIR}/vendor/png)

add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/headers)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/SimpleJson)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/zip)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/zlib)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/png)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/lua)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/nana)

# FIX silly vendor supplied cmake issues!
if(UNIX)
    target_compile_options(nana PRIVATE -Wno-reorder -Wno-pessimizing-move)
endif()

add_executable(factorio_data_browser data_dump.cpp)
target_link_libraries(factorio_data_browser PUBLIC options Lua nana headers
                                                   SimpleJson zip)
