project(Lua CXX)

message("Adding Static Library Lua")

file(GLOB_RECURSE SOURCE_FILES *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h)

list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/lua.cpp
     ${CMAKE_CURRENT_LIST_DIR}/luac.cpp)

list(REMOVE_ITEM HEADER_FILES ${CMAKE_CURRENT_LIST_DIR}/override_printf.h)

set_source_files_properties(${SOURCE_FILES} PROPERTIES LANGUAGE CXX)

add_library(Lua STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(Lua INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(
  Lua
  PRIVATE -DLUA_COMPAT_ALL
          # -DLUA_USE_POSIX
          -DLUA_USE_STRTODHEX -DLUA_USE_AFORMAT -DLUA_USE_LONGLONG)

if(ENABLE_LUA_DEBUGGER)
  target_compile_definitions(Lua PUBLIC -DUSE_LUA_IO -DUSE_LUA_COROUTINE)
endif()

# target_compile_options(Lua PRIVATE /FI
# ${CMAKE_CURRENT_LIST_DIR}/override_printf.h )

# target_link_libraries(Lua trio m)
