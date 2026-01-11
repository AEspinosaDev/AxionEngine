#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/Rasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreInitializationTest.log" );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Core Init Test" } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "TestScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.common.name = "TestRasterizer";

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // Asset Loading
        auto cubeHandle = assets.mesh( "Cube" ).createCube();
        // assets.deleteMesh( cubeHandle );

        auto ajaxHandle = assets.mesh( "Ajax" ).import( AXION_MESH_DIR "/ajax.obj" );

        // Scene Setup
        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();

        auto ajaxEntity = scene.createEntity( "Ajax" );
        ajaxEntity.addComponent<Core::Scene::MeshComponent>( cubeHandle );

        // Checks if ECS works right
        // {
        //     auto meshCompVal = ajaxEntity.getComponent<Core::Scene::MeshComponent>().mesh;
        //     if ( meshCompVal != ajaxHandle )
        //         return EXIT_FAILURE;

        //     auto transform0 = ajaxEntity.getComponent<Core::Scene::TransformComponent>();
        //     ajaxEntity.getComponent<Core::Scene::TransformComponent>().translate( { 10.0f, 0.0f, 0.0f } );
        //     auto transform1 = ajaxEntity.getComponent<Core::Scene::TransformComponent>();
        //     if ( transform0.translation == transform1.translation )
        //         return EXIT_FAILURE;
        // }

        // scene.destroyEntity( entity2 );

        while ( !wnd.shouldClose() )
        {
            wnd.update();
            rasterizer->render( scene, cameraEntity );
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
