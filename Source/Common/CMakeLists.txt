# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Common module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/Memory/*.cpp"
)


add_library(AxionCommon STATIC ${COMMON_SOURCES})


target_include_directories(AxionCommon
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
)

target_link_libraries(AxionCommon PUBLIC glm fmt::fmt)

# RESOLVE RESOURCES & SHADER SOURCES PATH
if(AXION_DISTRIBUTION)
    # Relative to the executable folder of a package: <Package>/bin/<platform>/ (see CMake/AddDistribution.cmake)
    target_compile_definitions(AxionCommon PUBLIC
        AXION_DISTRIBUTION
        AXION_MESH_DIR="../../assets/Meshes"
        AXION_TEXTURE_DIR="../../assets/Textures"
        AXION_SHADER_DIR="../../engine/shaders"
        AXION_SHADER_CACHE_DIR="../../engine/shaders"
    )
else()
    target_compile_definitions(AxionCommon PUBLIC
        AXION_MESH_DIR="${CMAKE_SOURCE_DIR}/Resources/Meshes"
        AXION_TEXTURE_DIR="${CMAKE_SOURCE_DIR}/Resources/Textures"
        AXION_SHADER_DIR="${CMAKE_SOURCE_DIR}/Source/Shaders"
        AXION_SHADER_CACHE_DIR="${CMAKE_SOURCE_DIR}/Build/ShaderCache"
    )
endif()

# PUBLIC: it changes engine interfaces, every target must see the same definition
if(AXION_BUILD_TOOLS)
    target_compile_definitions(AxionCommon PUBLIC AXION_TOOLS_BUILD)
endif()