
if(AXION_DISTRIBUTION)
    set(ABS_TEST_RESOURCE_DIR "../../assets/Tests")
else()
    get_filename_component(ABS_TEST_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources" ABSOLUTE)
endif()

add_library(TestsConfig INTERFACE)
target_compile_definitions(TestsConfig INTERFACE
    AXION_TESTS_RESOURCE_DIR="${ABS_TEST_RESOURCE_DIR}"
)

set(CMAKE_FOLDER "Tests")

# FUNCTIONAL TESTS
add_subdirectory(Func/GFX_ClearBackbuffer)
if(NOT AXION_DISTRIBUTION) # Compiles its own shaders at runtime: distribution builds have no shader compiler
    add_subdirectory(Func/GFX_UpdateAccels)
endif()
add_subdirectory(Func/Core_Stress)
add_subdirectory(Func/Core_GUI)

# UNIT TESTS

unset(CMAKE_FOLDER)