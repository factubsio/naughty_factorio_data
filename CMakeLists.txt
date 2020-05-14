cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

project(factorio-data-browser)

# stupid hacky nana doesn't like to be include() so fuck them
include_directories(nana/include)

add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0 _NO_DEBUG_HEAP=1)

include(${CMAKE_SOURCE_DIR}/src/CMakeLists.txt)
include(${CMAKE_SOURCE_DIR}/nana/CMakeLists.txt)

set(exec_name factorio_data_dumper)
add_executable(${exec_name} data_dump.cpp zip.c JSON.cpp JSONValue.cpp)
target_link_libraries(${exec_name} Lua nana)
target_include_directories(${exec_name} PUBLIC src)
