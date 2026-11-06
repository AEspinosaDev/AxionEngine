add_executable(CoreGUITest  main.cpp)
target_link_libraries(CoreGUITest PRIVATE AxionCore TestsConfig)
add_test(NAME CoreGUITest COMMAND CoreGUITest)
axion_setup_executable(CoreGUITest "Tests")

axion_add_distribution(CoreGUITest
    ASSETS_ROOT ${CMAKE_SOURCE_DIR}/Resources
    ASSETS Textures/Jupiter.jpg
)
