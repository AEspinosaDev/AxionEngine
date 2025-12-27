# cmake/AddExecutable.cmake

# =============================================================================
# Helper Function: axion_setup_executable
# -----------------------------------------------------------------------------
# Purpose:
#   1. Links the target with the Slang library automatically.
#   2. Organizes the output binaries into a clean structure (bin/Samples, bin/Editor, etc.).
#   3. Automatically copies the required slang.dll to the output folder (Windows only).
#
# Usage:
#   axion_setup_executable(MyTargetName "FolderName")
# =============================================================================
function(axion_setup_executable target_name folder_name)

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Target not found: ${target_name}")
    endif()

    target_link_libraries(${target_name} PRIVATE Slang::Slang)

    # 1. Output Directory Configuration
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${folder_name}"
    )
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/bin/${folder_name}"
        )
    endforeach()

    # 2. Refined DLL Copy (Only copy what is strictly necessary)
    if(WIN32)
        # Define the specific list of DLLs you need. 
        # usually: slang.dll + slang-glslang.dll (for spirv) + slang-compiler.dll
        set(SLANG_DLLS_TO_COPY 
            "${SLANG_BIN_DIR}/slang.dll"
            "${SLANG_BIN_DIR}/slang-glslang.dll" 
        )

        # Check if compiler dll exists (needed if compile at runtime)
        if(EXISTS "${SLANG_BIN_DIR}/slang-compiler.dll")
            list(APPEND SLANG_DLLS_TO_COPY "${SLANG_BIN_DIR}/slang-compiler.dll")
        endif()

        # Create a copy command for EACH file in the list
        foreach(DLL_PATH ${SLANG_DLLS_TO_COPY})
            get_filename_component(DLL_NAME ${DLL_PATH} NAME)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL_PATH}"
                "$<TARGET_FILE_DIR:${target_name}>/${DLL_NAME}"
                COMMENT "Copying ${DLL_NAME}..."
            )
        endforeach()
    endif()

endfunction()