add_executable(CoreStressTest  main.cpp)
target_link_libraries(CoreStressTest PRIVATE AxionCore TestsConfig)
add_test(NAME CoreStressTest COMMAND CoreStressTest)
axion_setup_executable(CoreStressTest "Tests")

axion_add_distribution(CoreStressTest
    ASSETS_ROOT ${CMAKE_SOURCE_DIR}/Resources
    ASSETS Textures/Jupiter.jpg
)
