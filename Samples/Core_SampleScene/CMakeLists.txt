
add_executable(CoreSampleScene  main.cpp)
target_link_libraries(CoreSampleScene PRIVATE SamplesConfig AxionCore)
axion_setup_executable(CoreSampleScene "Samples")


