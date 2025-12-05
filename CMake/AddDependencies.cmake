# =============================================================================
# Helper for Axion Engine to handle third-party libraries
# =============================================================================
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
# Slang (prebuilt from VulkanSDK)
# ----------------------------------------------------------------------------
set(SLANG_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include/slang")
if(NOT EXISTS "${SLANG_INCLUDE_DIR}")
    message(FATAL_ERROR "Slang not found inside Vulkan SDK! Update Vulkan SDK to 1.4.x+")
endif()
# Slang libraries
set(SLANG_LIB_DIR     "$ENV{VULKAN_SDK}/Lib")
set(SLANG_LIBS
    "$ENV{VULKAN_SDK}/Lib/slang.lib"
    "$ENV{VULKAN_SDK}/Lib/slangd.lib"
    "$ENV{VULKAN_SDK}/Lib/slang-rt.lib"
    "$ENV{VULKAN_SDK}/Lib/slang-rtd.lib"
)
# For convenience, expose to dependents
add_library(Slang INTERFACE)
target_include_directories(Slang INTERFACE ${SLANG_INCLUDE_DIR})
target_link_directories(Slang INTERFACE ${SLANG_LIB_DIR})
target_link_libraries(Slang INTERFACE ${SLANG_LIBS})
message(STATUS "Slang include: ${SLANG_INCLUDE_DIR}")
message(STATUS "Slang libs: ${SLANG_LIBS}")
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