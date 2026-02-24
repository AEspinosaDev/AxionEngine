# =============================================================================
# Helper for Axion Engine to handle third-party libraries
# =============================================================================
include(FetchContent)

set(CMAKE_FOLDER "ThirdParty")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# ----------------------------------------------------------------------------
# Vulkan (prebuilt from VulkanSDK)
# ----------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
message(FATAL_ERROR "Vulkan SDK not found! Install Vulkan SDK 1.3.296+")
endif()
message(STATUS "Using Vulkan SDK at: ${Vulkan_INCLUDE_DIRS}")
# ----------------------------------------------------------------------------
# Slang (Custom Prebuilt)
# ----------------------------------------------------------------------------
set(SLANG_VERSION "2025.24.2")

if(WIN32)
    set(SLANG_FILE "slang-${SLANG_VERSION}-windows-x86_64.zip")
elseif(UNIX AND NOT APPLE)
    set(SLANG_FILE "slang-${SLANG_VERSION}-linux-x86_64.zip")
endif()

set(SLANG_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_FILE}")

message(STATUS "Downloading Slang ${SLANG_VERSION} from: ${SLANG_URL}")

FetchContent_Declare(
    slang_package
    URL ${SLANG_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE 
)
# Unzip
FetchContent_MakeAvailable(slang_package)

set(SLANG_ROOT "${slang_package_SOURCE_DIR}")

if(NOT TARGET Slang::Slang)
    add_library(Slang::Slang SHARED IMPORTED GLOBAL)
endif()

if(WIN32)
    set_target_properties(Slang::Slang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_ROOT}/include"
    )
    set_target_properties(Slang::Slang PROPERTIES
        IMPORTED_IMPLIB "${SLANG_ROOT}/lib/slang.lib"
    )
    set(SLANG_BIN_DIR "${SLANG_ROOT}/bin" CACHE INTERNAL "Slang Binaries Folder")
    set(SLANG_DLL_SOURCE "${SLANG_ROOT}/bin/slang.dll" CACHE INTERNAL "Path to slang.dll")

    message(STATUS "Configuring Slang in: ${SLANG_ROOT}")
else()
    # (Soporte Linux comentado por ahora...)
endif()


# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/slang)
# add_library(Slang ALIAS Slang::Slang)
# message(STATUS "Slang Target configured via ThirdParty/slang")
# ----------------------------------------------------------------------------
# Shaderc (prebuilt from VulkanSDK)
# ----------------------------------------------------------------------------
# *****
# ----------------------------------------------------------------------------
# GLM (header-only)
# ----------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/glm-1.0.2)
# ----------------------------------------------------------------------------
# GLFW (optional, compiled)
# ----------------------------------------------------------------------------
option(BUILD_GLFW "Build GLFW as part of this project" ON)
if(BUILD_GLFW)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/glfw)
endif()
# ----------------------------------------------------------------------------
# Fmt (header only)
# ----------------------------------------------------------------------------
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)       
set(FMT_TEST OFF CACHE BOOL "" FORCE)          
set(FMT_DOC OFF CACHE BOOL "" FORCE)           
set(FMT_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/fmt)
# ----------------------------------------------------------------------------
# STB - IMAGE 
# ----------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/stb_image)
# ----------------------------------------------------------------------------
# GEOMETRY LOADERS 
# ----------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/tiny_obj_loader)
# ----------------------------------------------------------------------------
# MESH OPTIMIZER
# ----------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/meshoptimizer)
# ----------------------------------------------------------------------------
# D3D12 VMA 
# ----------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/D3D12MemoryAllocator)

unset(CMAKE_FOLDER)