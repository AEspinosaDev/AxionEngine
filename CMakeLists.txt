# =========================================================================
# AXION ENGINE ROOT CONFIGURATION
# =========================================================================

cmake_minimum_required(VERSION 3.16)
project(AxionEngine)

# =========================================================================
# User Configuration
# =========================================================================
option(AXION_BUILD_EDITOR  "Enable building of the Editor Application" ON)
option(AXION_BUILD_CORE    "Enable building of the Core Module"        ON)
option(AXION_BUILD_GFX     "Enable building of the Graphics Module"    ON)
option(AXION_BUILD_TESTS   "Enable building of the tests"              ON)
option(AXION_BUILD_SAMPLES "Enable building of the samples"            ON)

# Logic: Editor needs Core and GFX
if(AXION_BUILD_EDITOR)
    message(STATUS "Editor enabled: Forcing Core and GFX ON")
    set(AXION_BUILD_CORE ON CACHE BOOL "Forced by Editor" FORCE)
    set(AXION_BUILD_GFX  ON CACHE BOOL "Forced by Editor" FORCE)
endif()

# =========================================================================
# Compiler Settings
# =========================================================================
if (MSVC)
    add_compile_options(/W4 /Zi /std:c++20)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS) 
else()
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# =========================================================================
# Dependencies & Modules
# =========================================================================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

# Tools
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating Axion documentation"
        VERBATIM
    )
endif()

include(AddExecutable)
include(AddDependencies)

# =========================================================================
# Subdirectories
# =========================================================================

# 1. Common (Base type definitions, always needed)
add_subdirectory(Source/Common)

# 2. Graphics & RHI
if(AXION_BUILD_GFX)
    message(STATUS "Configuring Axion Graphics Module")
    add_subdirectory(Source/Graphics)
endif()

# 3. Core (Engine logic, Scene, ECS, etc.)
if(AXION_BUILD_CORE)
    message(STATUS "Configuring Axion Core Module")
    add_subdirectory(Source/EngineCore)
endif()

# 4. Editor 
if(AXION_BUILD_EDITOR)
    message(STATUS "Configuring Axion Editor Application")
    add_subdirectory(Source/Editor)
endif()

# 5. Samples
if(AXION_BUILD_SAMPLES)
    message(STATUS "Configuring Axion Samples")
    add_subdirectory(Samples)
endif()

# 6. Tests
if(AXION_BUILD_TESTS)
    message(STATUS "Configuring Axion Tests")
    enable_testing() # CTest
    add_subdirectory(Tests)
endif()