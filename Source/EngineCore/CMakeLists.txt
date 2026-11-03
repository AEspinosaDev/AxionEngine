# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Core Module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB CORE_SOURCES
    "src/*.hpp"
    "src/*.cpp"
    # Scene
    "src/Scene/*.cpp"
    # Platform
    "src/Platform/*.cpp"
    # Assets
    "src/Assets/*.cpp"
    "src/Assets/Loaders/*.cpp"
    # Render
    "src/Render/*.cpp"
    "src/Render/Rasterizer/*.cpp"
)

add_library(AxionCore STATIC ${CORE_SOURCES})


target_include_directories(AxionCore
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src  
)

# Link system libraries
target_link_libraries(AxionCore
PUBLIC
AxionCommon
imgui
PRIVATE
# Internal Modules
AxionGFX
AxionRHI
# External
tiny_obj_loader
stb_image
meshoptimizer
)

target_include_directories(AxionCore
    SYSTEM PRIVATE
        ${TINYOBJ_INCLUDE_DIR}
        ${STB_INCLUDE_DIR}
        ${MESHOPT_INCLUDE_DIR}
)
