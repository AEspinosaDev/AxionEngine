
if(AXION_DISTRIBUTION)
    set(ABS_RESOURCE_DIR "../../assets/Samples")
else()
    get_filename_component(ABS_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources" ABSOLUTE)
endif()

add_library(SamplesConfig INTERFACE)
target_compile_definitions(SamplesConfig INTERFACE
    AXION_SAMPLES_RESOURCE_DIR="${ABS_RESOURCE_DIR}"
)

set(CMAKE_FOLDER "Samples")

# Core Module (Core)
add_subdirectory(Core_SampleScene)

# GFX and Interop samples compile their own shaders at runtime: distribution builds have no shader compiler
if(NOT AXION_DISTRIBUTION)
    # Graphics Module (GFX)
    add_subdirectory(GFX_Triangle)
    add_subdirectory(GFX_Compute)
    add_subdirectory(GFX_Raytracing)
    add_subdirectory(GFX_VertexPulling)

    # Interop (Core + GFX)
    add_subdirectory(Interop_TextureSamplers)
    add_subdirectory(Interop_MeshShading)
endif()

# Demos (Entire Apps)

unset(CMAKE_FOLDER)
