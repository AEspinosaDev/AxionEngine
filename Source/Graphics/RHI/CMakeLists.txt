# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Graphics::RHI submodule
# /////////////////////////////////////////////////////////////////////////////

file(GLOB RHI_SOURCES CONFIGURE_DEPENDS
    "src/DX12/*.cpp"
    "src/DX12/*.hpp"
    "src/DX12/*.h"
    "src/*.h"
    "src/*.cpp"
)

if(AXION_ENABLE_VULKAN)
    file(GLOB RHI_VULKAN_SOURCES CONFIGURE_DEPENDS
        "src/Vulkan/*.cpp"
        "src/Vulkan/*.hpp"
        "src/Vulkan/*.h"
    )
    list(APPEND RHI_SOURCES ${RHI_VULKAN_SOURCES})
endif()

add_library(AxionRHI STATIC ${RHI_SOURCES})


target_include_directories(AxionRHI
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include
PRIVATE SYSTEM   
${CMAKE_SOURCE_DIR}/ThirdParty/directx-headers/include    
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src  
)

# Link system libraries
target_link_libraries(AxionRHI
PUBLIC
# Direct X12
dxgi.lib
d3d12.lib
d3dcompiler.lib
user32.lib
dxguid.lib
D3D12MA
#Internal Modules
AxionCommon
#imgui
imgui
)


if(AXION_ENABLE_VULKAN)
    target_include_directories(AxionRHI SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
    # All include headers + vulkan.lib only
    target_link_libraries(AxionRHI PUBLIC Vulkan::Vulkan)
endif()

if(BUILD_GLFW)
    target_link_libraries(AxionRHI PUBLIC glfw )
    target_compile_definitions(AxionRHI PRIVATE BUILD_GLFW)
endif()
