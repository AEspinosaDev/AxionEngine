add_executable(GFXUpdateAccelTest  main.cpp)
target_link_libraries(GFXUpdateAccelTest PRIVATE AxionGFX TestsConfig)
add_test(NAME GFXUpdateAccelTest COMMAND GFXUpdateAccelTest)
