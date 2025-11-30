
add_executable(GFXComputeTest  main.cpp)
target_link_libraries(GFXComputeTest PRIVATE AxionGFX)
add_test(NAME GFXComputeTest COMMAND GFXComputeTest)


