
add_executable(GFXSamplersTest  main.cpp)
target_link_libraries(GFXSamplersTest PRIVATE AxionGFX)
add_test(NAME GFXSamplersTest COMMAND GFXSamplersTest)

target_compile_definitions(GFXSamplersTest PRIVATE
    SAMPLER_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)