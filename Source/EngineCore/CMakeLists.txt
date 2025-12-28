# /////////////////////////////////////////////////////////////////////////////
# Axion Engine Core Module
# /////////////////////////////////////////////////////////////////////////////

file(GLOB CORE_SOURCES
    "src/*.hpp"
    "src/*.cpp"
    "src/Scene/*.cpp"
)

add_library(AxionCore STATIC ${CORE_SOURCES})


target_include_directories(AxionCore
PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/include   
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/src  
)

# Link system libraries
target_link_libraries(AxionCore
PUBLIC
#Internal Modules
AxionCommon
PRIVATE
AxionGFX
AxionRHI
)
