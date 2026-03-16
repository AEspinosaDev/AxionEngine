# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Graphics::RHI submodule
# /////////////////////////////////////////////////////////////////////////////

file(GLOB RHI_SOURCES
    "src/DX12/*.cpp"
    "src/DX12/*.hpp"
    "src/DX12/*.h"
    "src/Vulkan/*.cpp"
    "src/Vulkan/*.hpp"
    "src/Vulkan/*.h"
    "src/*.h"
    "src/*.cpp"
)

add_library(AxionRHI STATIC ${RHI_SOURCES})


target_include_directories(AxionRHI
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
${CMAKE_SOURCE_DIR}/ThirdParty/directx-headers/include    
${Vulkan_INCLUDE_DIRS}
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
# Vulkan
# All include headers + vulkan.lib only
Vulkan::Vulkan 
#Internal Modules
AxionCommon
#imgui
imgui
)

# Si quieres flags específicos por backend
if(BUILD_GLFW)
    target_link_libraries(AxionRHI PUBLIC glfw )
    target_compile_definitions(AxionRHI PRIVATE BUILD_GLFW)
endif()
