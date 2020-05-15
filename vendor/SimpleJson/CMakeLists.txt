add_library(SimpleJson STATIC JSON.cpp JSONValue.cpp)
message("Adding Static Library SimpleJson")
target_include_directories(SimpleJson INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
