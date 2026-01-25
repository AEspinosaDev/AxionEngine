add_executable(CoreStressTest  main.cpp)
target_link_libraries(CoreStressTest PRIVATE AxionCore TestsConfig)
add_test(NAME CoreStressTest COMMAND CoreStressTest)
axion_setup_executable(CoreStressTest "Tests")
