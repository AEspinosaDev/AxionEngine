#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/UnlitMaterial.h"
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
        rastDesc.common.name             = "TestRasterizer";
        rastDesc.common.selectedDeviceID = 0;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // Asset Loading
        auto cubeHandle   = assets.mesh( "Cube" ).createCube();
        auto sphreHandle  = assets.mesh( "Sphere" ).createSphere();
        auto dragonHandle = assets.mesh( "Dragon" ).import( AXION_MESH_DIR "/dragon.obj" );
        auto ajaxHandle   = assets.mesh( "Ajax" ).import( AXION_MESH_DIR "/ajax.obj" );

        auto unlitHandle = assets.material( "Unlit" ).create<Core::Assets::UnlitMaterial>();
        auto unlitMat    = assets.getMaterial<Core::Assets::UnlitMaterial>( unlitHandle );
        unlitMat->setColor( { 0.1f, 0.0f, 0.6f } );

        // Scene Setup
        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();
        cameraEntity.getComponent<Core::Scene::TransformComponent>().position( { 0.0f, 0.0f, -4.0f } );
        cameraEntity.getComponent<Core::Scene::TransformComponent>().lookAt( { 0.0f, 0.0f, 0.0f } );

        auto cubeEntity = scene.createEntity( "Cube" );
        cubeEntity.addComponent<Core::Scene::MeshComponent>( cubeHandle, unlitHandle );
        cubeEntity.getComponent<Core::Scene::TransformComponent>().translation = { -1.0f, -1.0f, 0.0f };

        auto sphereEntity = scene.createEntity( "Sphere" );
        sphereEntity.addComponent<Core::Scene::MeshComponent>( cubeHandle, unlitHandle );
        sphereEntity.getComponent<Core::Scene::TransformComponent>().translation = { 1.0f, 1.0f, 0.0f };
        sphereEntity.getComponent<Core::Scene::TransformComponent>().scale       = { 0.5f, 0.5f, 0.5f };

        auto ajaxEntity = scene.createEntity( "Ajax" );
        ajaxEntity.addComponent<Core::Scene::MeshComponent>( ajaxHandle, unlitHandle );
        ajaxEntity.getComponent<Core::Scene::TransformComponent>().translation = { 1.0f, -1.0f, 0.0f };

        auto dragonEntity = scene.createEntity( "Dragon" );
        dragonEntity.addComponent<Core::Scene::MeshComponent>( dragonHandle, unlitHandle );
        dragonEntity.getComponent<Core::Scene::TransformComponent>().translation = { -1.0f, 1.0f, 0.0f };

        auto eraseEvent = wnd.onKey().subscribe( [&]( const Event::KeyEvent& e ) {
            if ( e.keyCode == Event::KeyCode::W && e.pressed )
                scene.destroyEntity( sphereEntity );
        } );

        static auto startTime = std::chrono::high_resolution_clock::now();
        while ( !wnd.shouldClose() )
        {

            static uint64_t                           frameCounter   = 0;
            static double                             elapsedSeconds = 0.0;
            static std::chrono::high_resolution_clock clock;
            static auto                               t0 = clock.now();

            frameCounter++;
            auto t1        = clock.now();
            auto deltaTime = t1 - t0;
            t0             = t1;

            elapsedSeconds += deltaTime.count() * 1e-9;
            if ( elapsedSeconds > 1.0 )
            {
                wchar_t buffer[100];
                double  fps = frameCounter / elapsedSeconds;
                swprintf_s( buffer, 100, L"FPS: %.2f\n", fps );

                OutputDebugStringW( buffer );

                frameCounter   = 0;
                elapsedSeconds = 0.0;
            }
            float time = std::chrono::duration<float>( t1 - startTime ).count();

            // float angle = time * 1.0f;
            // cubeEntity.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, angle, 0.0f } );
            // cubeEntity2.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, 0.0f, angle } );

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
