/**
 * @file DistributionWorkingDirectory.cpp
 * @brief Compiled into the packaged executables by axion_add_distribution.
 * The engine path macros are relative literals (eg: AXION_MESH_DIR "/dragon.obj"), so they depend on the working directory.
 * This makes them work wherever the package is launched from.
 */
#include <Windows.h>
#include <filesystem>

namespace {

/// @brief Moves the working directory to the executable folder. Runs before main.
const bool WORKING_DIRECTORY_READY = []() {
    wchar_t     path[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW( nullptr, path, ARRAYSIZE( path ) );
    if ( length == 0 || length == ARRAYSIZE( path ) )
        return false;

    std::error_code error;
    std::filesystem::current_path( std::filesystem::path( path ).parent_path(), error );
    return !error;
}();

} // namespace
