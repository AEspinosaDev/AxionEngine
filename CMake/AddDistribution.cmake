# cmake/AddDistribution.cmake

set(AXION_PACKAGE_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/PackageDistribution.cmake")
set(AXION_PACKAGE_WORKING_DIRECTORY_SOURCE "${CMAKE_CURRENT_LIST_DIR}/DistributionWorkingDirectory.cpp")

# =============================================================================
# Helper Function: axion_add_distribution
# -----------------------------------------------------------------------------
# Purpose:
#   Creates the target Package_<target>, which generates a self-contained package of an
#   application in Dist/<package name>, plus a zip of it next to the folder:
#
#     <package name>/
#       bin/<platform>/   Executable and Visual C++ runtime
#       engine/shaders/   Shader cache taken from Build/ShaderCache (see Scripts/CookShadersDXIL.bat)
#       assets/           Assets of the application, keeping their path relative to ASSETS_ROOT
#
#   It has no effect without AXION_DISTRIBUTION, so it can stay in the CMakeLists of any application.
#   The application must take its shaders from the shader cache: distribution builds have no shader compiler.
#
# Usage:
#   axion_add_distribution(MyTargetName
#       [PACKAGE_NAME MyPackage]                    # Default: the target name
#       [ASSETS_ROOT ${CMAKE_SOURCE_DIR}/Resources] # Base folder of ASSETS
#       [ASSETS Meshes/dragon.obj Textures]         # Files or folders
#   )
# =============================================================================
function(axion_add_distribution target_name)

    if(NOT AXION_DISTRIBUTION)
        return()
    endif()

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Target not found: ${target_name}")
    endif()

    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "PACKAGE_NAME;ASSETS_ROOT" "ASSETS")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "axion_add_distribution(${target_name}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_PACKAGE_NAME)
        set(ARG_PACKAGE_NAME ${target_name})
    endif()

    # 1. Platform of this build
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PLATFORM "win64")
        set(SHADER_FORMAT "dxil")
    else()
        message(FATAL_ERROR "axion_add_distribution(${target_name}): only 64 bit Windows is supported")
    endif()

    # 2. Check the assets now: a typo must not wait until packaging
    if(ARG_ASSETS AND NOT ARG_ASSETS_ROOT)
        message(FATAL_ERROR "axion_add_distribution(${target_name}): ASSETS needs ASSETS_ROOT")
    endif()
    foreach(ASSET IN LISTS ARG_ASSETS)
        if(NOT EXISTS "${ARG_ASSETS_ROOT}/${ASSET}")
            message(FATAL_ERROR "axion_add_distribution(${target_name}): asset not found: ${ARG_ASSETS_ROOT}/${ASSET}")
        endif()
    endforeach()

    # 3. Visual C++ runtime: a clean machine might not have it installed
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    set(RUNTIME_LIBS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
    list(FILTER RUNTIME_LIBS INCLUDE REGEX "/(msvcp|vcruntime)[^/]*\\.dll$")
    if(NOT RUNTIME_LIBS)
        message(FATAL_ERROR "axion_add_distribution(${target_name}): Visual C++ runtime not found")
    endif()

    # 4. Paths are relative to the executable folder, wherever the package is launched from
    target_sources(${target_name} PRIVATE "${AXION_PACKAGE_WORKING_DIRECTORY_SOURCE}")

    # 5. Packaging target
    add_custom_target(Package_${target_name}
        COMMAND ${CMAKE_COMMAND}
            "-DPACKAGE_NAME=${ARG_PACKAGE_NAME}"
            "-DPLATFORM=${PLATFORM}"
            "-DOUTPUT_DIR=${CMAKE_SOURCE_DIR}/Dist"
            "-DEXECUTABLE=$<TARGET_FILE:${target_name}>"
            "-DRUNTIME_LIBS=${RUNTIME_LIBS}"
            "-DSHADER_CACHE_DIR=${CMAKE_SOURCE_DIR}/Build/ShaderCache"
            "-DSHADER_FORMAT=${SHADER_FORMAT}"
            "-DASSETS_ROOT=${ARG_ASSETS_ROOT}"
            "-DASSETS=${ARG_ASSETS}"
            -P "${AXION_PACKAGE_SCRIPT}"
        DEPENDS ${target_name}
        COMMENT "Packaging ${ARG_PACKAGE_NAME} for ${PLATFORM}"
        VERBATIM
    )
    set_target_properties(Package_${target_name} PROPERTIES FOLDER "Distribution")

endfunction()
