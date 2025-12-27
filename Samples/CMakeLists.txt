
get_filename_component(ABS_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources" ABSOLUTE)

add_library(SamplesConfig INTERFACE)
target_compile_definitions(SamplesConfig INTERFACE
    AXION_SAMPLES_RESOURCE_DIR="${ABS_RESOURCE_DIR}"
)

set(CMAKE_FOLDER "Samples")

# Graphics Module (GFX)
add_subdirectory(GFX_Raster)
add_subdirectory(GFX_Compute)
add_subdirectory(GFX_TextureSamplers)
add_subdirectory(GFX_Raytracing)
add_subdirectory(GFX_VertexPulling)

# Core Module (Core)

# Editor 

unset(CMAKE_FOLDER)
