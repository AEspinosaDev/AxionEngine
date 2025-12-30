#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Platform/Window.h"

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreInitializationTest.log" );
#endif

        Core::Platform::Window wnd( { .platformType = Graphics::PlatformType::GLFW,
                                      .name         = "Core Init Test" } );

        while ( !wnd.shouldClose() )
        {
            wnd.update();
        }
    } catch ( const std::exception& e )
    {
        return EXIT_FAILURE;
    }
#ifdef AXION_DEBUG
    Axion::Logger::shutdown();
#endif

    return EXIT_SUCCESS;
}
