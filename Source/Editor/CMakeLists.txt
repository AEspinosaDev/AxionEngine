   
file(GLOB APP_SOURCES
    "src/*.cpp"
)

add_executable(AxionEditor ${APP_SOURCES})

target_link_libraries(AxionEditor
   AxionCommon
   AxionGFX
)


set_target_properties(AxionEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bin"
)

# axion_setup_executable(AxionEditor "Editor")