
add_executable(CoreParticleSystem  main.cpp)
target_link_libraries(CoreParticleSystem PRIVATE SamplesConfig AxionCore)
axion_setup_executable(CoreParticleSystem "Samples")

axion_add_distribution(CoreParticleSystem
    ASSETS_ROOT ${CMAKE_SOURCE_DIR}/Resources
    ASSETS Textures/Jupiter.jpg Textures/asteroid.png Meshes/asteroid.obj
)
