# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Graphics Upper Module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS
    "src/*.hpp"
    "src/*.cpp"
    "src/Platforms/*.cpp"
    "src/Platforms/*.cpp"
    "src/Subsystems/*.hpp"
    "src/Subsystems/*.cpp"
)

# Distribution builds have no shader compiler: their shaders only come from the shader cache
if(AXION_DISTRIBUTION)
    list(FILTER COMMON_SOURCES EXCLUDE REGEX "ShaderCompiler\\.cpp$")
endif()

# ADD SUB-MODULE FOR RHI
add_subdirectory(RHI)

add_library(AxionGFX STATIC ${COMMON_SOURCES})


target_include_directories(AxionGFX
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
)

target_link_libraries(AxionGFX 
PUBLIC
AxionCommon
AxionRHI
PRIVATE
nlohmann_json)

if(NOT AXION_DISTRIBUTION)
    target_link_libraries(AxionGFX PUBLIC Slang::Slang)
endif()

# Shaders are always compiled with the DXC of the Windows SDK, whatever the PATH finds first
if(AXION_DXC_DIR)
    target_compile_definitions(AxionGFX PRIVATE AXION_DXC_DIR="${AXION_DXC_DIR}")
endif()



