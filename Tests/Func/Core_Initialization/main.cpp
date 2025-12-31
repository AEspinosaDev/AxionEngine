#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Platform/Window.h"

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreInitializationTest.log", Logger::Module::Core );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Core Init Test" } );
        Core::Assets::AssetManager assets;
        auto cubeHandle = assets.createCube( "Cube" );
        assets.deleteMesh( cubeHandle );
        auto ajaxHandle = assets.importMesh( "AJAX",  AXION_TESTS_RESOURCE_DIR "/ajax.obj" );
        auto ajaxMesh = assets.getMesh( ajaxHandle );


        // while ( !wnd.shouldClose() )
        // {
        //     wnd.update();
        // }
    } catch ( const std::exception& e )
    {
        return EXIT_FAILURE;
    }
#ifdef AXION_DEBUG
    Axion::Logger::shutdown();
#endif

    return EXIT_SUCCESS;
}
