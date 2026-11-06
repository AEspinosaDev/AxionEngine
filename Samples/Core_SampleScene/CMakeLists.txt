
add_executable(CoreSampleScene  main.cpp)
target_link_libraries(CoreSampleScene PRIVATE SamplesConfig AxionCore)
axion_setup_executable(CoreSampleScene "Samples")

axion_add_distribution(CoreSampleScene
    ASSETS_ROOT ${CMAKE_SOURCE_DIR}/Resources
    ASSETS Meshes/dragon.obj Meshes/ajax.obj Meshes/ajax.mtl Textures
)


