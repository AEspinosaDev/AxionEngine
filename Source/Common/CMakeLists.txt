# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Common module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB COMMON_SOURCES
    "src/*.cpp"
    "src/Memory/*.cpp"
)


add_library(AxionCommon STATIC ${COMMON_SOURCES})


target_include_directories(AxionCommon
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
)

target_link_libraries(AxionCommon PUBLIC glm fmt::fmt)

# RESOLVE RESOURCES PATH
target_compile_definitions(AxionCommon PUBLIC
    AXION_MESH_DIR="${CMAKE_SOURCE_DIR}/Resources/Meshes"
    AXION_TEXTURE_DIR="${CMAKE_SOURCE_DIR}/Resources/Textures"
    AXION_SHADER_DIR="${CMAKE_SOURCE_DIR}/Resources/Shaders"
)