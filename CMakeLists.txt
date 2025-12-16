# =========================================================================
# AXION ENGINE ROOT CONFIGURATION
# =========================================================================

cmake_minimum_required(VERSION 3.16)
project(AxionEngine)


# =========================================================================
# User Configuration
# =========================================================================
option(AXION_ENABLE_TESTS "Enable building of the tests" ON)
option(AXION_ENABLE_SAMPLES "Enable building of the samples" ON)


# Solo para MSVC, usa /std:c++20
if (MSVC)
    add_compile_options(/W4 /Zi /std:c++20)
else()
    # Para GCC/Clang
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# Doxigen for documentation
find_package(Doxygen)

set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Axion Engine API documentation with Doxygen"
    VERBATIM
    )

# CMake Modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
    
# =========================================================================
# Add External Libraries and Depdencies
# =========================================================================

include(AddDependencies)

# =========================================================================
# Engine Source
# =========================================================================

add_subdirectory(Source/Common)
add_subdirectory(Source/Graphics)
add_subdirectory(Source/EngineCore)
add_subdirectory(Source/Editor)

# =========================================================================
# Resources Configuration
# =========================================================================


# =========================================================================
# Samples Configuration
# =========================================================================
if(AXION_ENABLE_SAMPLES)
    add_subdirectory(Samples)
endif()
# =========================================================================
# Tests Configuration
# =========================================================================
if(AXION_ENABLE_TESTS)
    add_subdirectory(Tests)
endif()
