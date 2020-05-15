add_library(headers INTERFACE)
message("Adding Header Only Includes")
target_include_directories(headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
