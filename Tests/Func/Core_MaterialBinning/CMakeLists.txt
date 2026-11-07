add_executable(CoreMaterialBinningTest  main.cpp)
target_link_libraries(CoreMaterialBinningTest PRIVATE AxionCore TestsConfig)
add_test(NAME CoreMaterialBinningTest COMMAND CoreMaterialBinningTest)
axion_setup_executable(CoreMaterialBinningTest "Tests")

axion_add_distribution(CoreMaterialBinningTest
    ASSETS_ROOT ${CMAKE_SOURCE_DIR}/Resources
    ASSETS Textures/Jupiter.jpg Textures/asteroid.png Meshes/asteroid.obj
)
