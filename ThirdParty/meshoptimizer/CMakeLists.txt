cmake_minimum_required(VERSION 3.16)
project(meshoptimizer)
add_library(meshoptimizer)

file(GLOB MESH_OPT_SOURCES
    "src/*.cpp"
)
target_sources(meshoptimizer PRIVATE ${MESH_OPT_SOURCES})
target_include_directories(meshoptimizer SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")