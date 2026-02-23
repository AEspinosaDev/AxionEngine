cmake_minimum_required(VERSION 3.16)
project(D3D12MA)

add_library(D3D12MA)
target_sources(D3D12MA PRIVATE "src/D3D12MemAlloc.cpp")
target_include_directories(D3D12MA PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")