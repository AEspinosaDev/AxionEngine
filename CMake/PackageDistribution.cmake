# =============================================================================
# Generates the package of an application. Run by the Package_<target> targets (see AddDistribution.cmake).
# =============================================================================

# Check it first: a failure must not delete the package that is already there
file(GLOB SHADER_CACHE_FILES "${SHADER_CACHE_DIR}/*.${SHADER_FORMAT}.*")
if(NOT SHADER_CACHE_FILES)
    message(FATAL_ERROR "No ${SHADER_FORMAT} shader cache in ${SHADER_CACHE_DIR}. Generate it first with Scripts/CookShadersDXIL.bat")
endif()

set(PACKAGE_DIR "${OUTPUT_DIR}/${PACKAGE_NAME}")
set(ZIP_FILE "${OUTPUT_DIR}/${PACKAGE_NAME}-${PLATFORM}.zip")

# Start from scratch: nothing from a previous package can stay behind
file(REMOVE_RECURSE "${PACKAGE_DIR}")
file(REMOVE "${ZIP_FILE}")

file(COPY "${EXECUTABLE}" ${RUNTIME_LIBS} DESTINATION "${PACKAGE_DIR}/bin/${PLATFORM}")
file(COPY ${SHADER_CACHE_FILES} DESTINATION "${PACKAGE_DIR}/engine/shaders")

foreach(ASSET IN LISTS ASSETS)
    get_filename_component(ASSET_PARENT "${ASSET}" DIRECTORY)
    file(COPY "${ASSETS_ROOT}/${ASSET}" DESTINATION "${PACKAGE_DIR}/assets/${ASSET_PARENT}")
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${ZIP_FILE}" --format=zip -- "${PACKAGE_NAME}"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE ZIP_RESULT
)
if(NOT ZIP_RESULT EQUAL 0)
    message(FATAL_ERROR "Can't create ${ZIP_FILE}")
endif()

message(STATUS "Package ready: ${PACKAGE_DIR}")
message(STATUS "Zip ready: ${ZIP_FILE}")
