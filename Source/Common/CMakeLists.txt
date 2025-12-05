# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Common module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB COMMON_SOURCES
    "src/*.cpp"
)


add_library(AxionCommon STATIC ${COMMON_SOURCES})


target_include_directories(AxionCommon
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
)

target_link_libraries(AxionCommon PUBLIC glm fmt::fmt stb_image)

# RESOLVE RESOURCES PATH
target_compile_definitions(AxionCommon PUBLIC
    # AXION_MODEL_DIR="${CMAKE_SOURCE_DIR}/Shaders"
    # AXION_TEXTURE_DIR="${CMAKE_SOURCE_DIR}/Resources"
    AXION_SHADER_DIR="${CMAKE_SOURCE_DIR}/Resources/Shaders"
)