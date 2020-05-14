project(Lua CXX)

file(GLOB_RECURSE SOURCE_FILES *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h)

list(REMOVE_ITEM SOURCE_FILES
        ${CMAKE_CURRENT_LIST_DIR}/lua.cpp
        ${CMAKE_CURRENT_LIST_DIR}/luac.cpp
        )

set_source_files_properties(${SOURCE_FILES} PROPERTIES LANGUAGE CXX)

add_library(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(${PROJECT_NAME} PUBLIC
        -DLUA_COMPAT_ALL
        # -DLUA_USE_POSIX
        -DLUA_USE_STRTODHEX
        -DLUA_USE_AFORMAT
        -DLUA_USE_LONGLONG
)

if (ENABLE_LUA_DEBUGGER)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
            -DUSE_LUA_IO
            -DUSE_LUA_COROUTINE
    )
endif()

message(INFO ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(${PROJECT_NAME} PRIVATE
        /FI ${CMAKE_CURRENT_LIST_DIR}/override_printf.h
)

# target_link_libraries(${PROJECT_NAME} trio)
