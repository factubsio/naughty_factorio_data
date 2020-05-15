cmake_minimum_required(VERSION 3.10)
project(factorio-data-browser CXX C)

add_library(options INTERFACE)
target_compile_features(options INTERFACE cxx_std_17)

# stupid hacky nana doesn't like to be include() so fuck them
# include_directories(nana/include)

add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0 _NO_DEBUG_HEAP=1)

add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/headers)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/lua)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/SimpleJson)
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/zip)
add_subdirectory(${CMAKE_SOURCE_DIR}/nana)

add_executable(factorio_data_dumper data_dump.cpp)
target_link_libraries(factorio_data_dumper PUBLIC options Lua nana headers SimpleJson zip m)
# target_include_directories(factorio_data_dumper PUBLIC src)
