cmake_minimum_required(VERSION 3.15)
project(factorio-data-browser CXX C)

add_library(options INTERFACE)
target_compile_features(options INTERFACE cxx_std_17)

# stupid hacky nana doesn't like to be include() so fuck them
# include_directories(nana/include)

add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0 _NO_DEBUG_HEAP=1)


set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Tell png that we are building zlib alongside the project
SET(PNG_BUILD_ZLIB ${CMAKE_SOURCE_DIR}/vendor/zlib CACHE STRING "Build libfoo shared library" FORCE)
set(ZLIB_LIBRARY zlibstatic)
SET(PNG_SHARED OFF CACHE BOOL "blah" FORCE)
SET(PNG_TESTS  OFF CACHE BOOL "blah" FORCE)
SET(SKIP_INSTALL_EXPORT ON CACHE BOOL "blah" FORCE)

# I don't know how to set the include_directores before add_subdirectory png
# Otherwise we have to use ZLIB_ROOT but then cmake complains because it calls cmake_policy(3.1)
include_directories(${CMAKE_SOURCE_DIR}/vendor/zlib)
# Also need to have this for zconf.h
include_directories(${CMAKE_BINARY_DIR}/vendor/zlib)
# need this so nana can pick it up
include_directories(${CMAKE_SOURCE_DIR}/vendor/png)
# pnglibconf.h
include_directories(${CMAKE_BINARY_DIR}/vendor/png)

add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/headers)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/zlib)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/png)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/lua)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/SimpleJson)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/zip)
add_subdirectory(${CMAKE_SOURCE_DIR}/nana)

add_executable(factorio_data_dumper data_dump.cpp)
target_link_libraries(factorio_data_dumper PUBLIC options Lua nana headers SimpleJson zip)
# target_include_directories(factorio_data_dumper PUBLIC src)
