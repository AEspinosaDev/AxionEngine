
get_filename_component(ABS_TEST_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources" ABSOLUTE)

add_library(TestsConfig INTERFACE)
target_compile_definitions(TestsConfig INTERFACE
    AXION_TESTS_RESOURCE_DIR="${ABS_TEST_RESOURCE_DIR}"
)

set(CMAKE_FOLDER "Tests")

# FUNCTIONAL TESTS
add_subdirectory(Func/GFX_ClearBackbuffer)
add_subdirectory(Func/GFX_UpdateAccels)
add_subdirectory(Func/Core_Initialization)



# UNIT TESTS

unset(CMAKE_FOLDER)