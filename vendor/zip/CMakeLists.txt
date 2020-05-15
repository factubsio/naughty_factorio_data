add_library(zip STATIC zip.c)
message("Adding Static Library Zip")
target_include_directories(zip INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
