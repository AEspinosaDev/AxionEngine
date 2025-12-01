
add_executable(GFXRasterTest  main.cpp)
target_link_libraries(GFXRasterTest PRIVATE AxionGFX)
add_test(NAME GFXRasterTest COMMAND GFXRasterTest)


