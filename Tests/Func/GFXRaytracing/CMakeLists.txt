

add_executable(GFXRaytracingTest  main.cpp)
target_link_libraries(GFXRaytracingTest PRIVATE AxionGFX)
add_test(NAME GFXRaytracingTest COMMAND GFXRaytracingTest)

target_compile_definitions(GFXRaytracingTest PRIVATE
    RTX_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)