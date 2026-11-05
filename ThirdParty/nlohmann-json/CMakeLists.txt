cmake_minimum_required(VERSION 3.16)
project(nlohmann_json)

# Single header release of nlohmann/json v3.12.0
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
