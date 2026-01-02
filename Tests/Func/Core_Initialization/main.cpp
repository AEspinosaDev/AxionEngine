#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"

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
        Core::Scene::Scene         scene( "TestScene", &assets );

        auto cubeHandle = assets.mesh( "Cube" ).createCube();
        assets.deleteMesh( cubeHandle );
        auto ajaxHandle = assets.mesh( "Ajax" ).import( AXION_MESH_DIR "/ajax.obj" );
        auto ajaxMesh   = assets.getMesh( ajaxHandle );

        auto entity1 = scene.createEntity( "Entity1" );

        auto entity2 = scene.createEntity( "Ajax" );
        entity2.addComponent<Core::Scene::MeshComponent>( ajaxHandle );
        auto meshCompVal = entity2.getComponent<Core::Scene::MeshComponent>().mesh;
        auto transform0  = entity2.getComponent<Core::Scene::TransformComponent>();
        entity2.getComponent<Core::Scene::TransformComponent>().translate( { 10.0f, 0.0f, 0.0f } );
        auto transform1 = entity2.getComponent<Core::Scene::TransformComponent>();

        if ( meshCompVal != ajaxHandle )
            return EXIT_FAILURE;

        if ( transform0.translation == transform1.translation )
            return EXIT_FAILURE;

        scene.destroyEntity( entity2 );



        // while (
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
