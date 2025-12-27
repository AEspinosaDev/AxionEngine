# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Graphics Upper Module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB COMMON_SOURCES
    "src/*.hpp"
    "src/*.cpp"
    "src/Platforms/*.cpp"
    "src/Platforms/*.cpp"
    "src/Subsystems/*.hpp"
    "src/Subsystems/*.cpp"
)

# ADD SUB-MODULE FOR RHI
add_subdirectory(RHI)

add_library(AxionGFX STATIC ${COMMON_SOURCES})


target_include_directories(AxionGFX
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
)

target_link_libraries(AxionGFX 
PUBLIC
Slang::Slang
AxionCommon
AxionRHI)



