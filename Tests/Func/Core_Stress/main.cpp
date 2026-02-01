#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/DebugMaterial.h"
#include "Axion/Core/Assets/Materials/UnlitMaterial.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/Rasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"
#include <random> // Necesario para la generación aleatoria
#include <vector>

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreInitializationTest.log" );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Core Stress Test - Thousands of objects" } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "TestScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.useGPUCulling             = true;
        rastDesc.common.name               = "TestRasterizer";
        rastDesc.common.selectedDeviceID   = 0;
        rastDesc.memory.volatileBufferSize = 1024 * 1024 * 64;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // 1. ASSETS
        // -------------------------------------------------------------------------
        auto cubeHandle   = assets.mesh( "Cube" ).createCube();
        auto sphereHandle = assets.mesh( "Sphere" ).createSphere();
        // auto dragonHandle = assets.mesh( "Dragon" ).import( AXION_MESH_DIR "/dragon.obj" );
        // auto ajaxHandle   = assets.mesh( "Ajax" ).import( AXION_MESH_DIR "/ajax.obj" );

        // Materials
        auto unlitHandle = assets.material( "UnlitRed" ).create<Core::Assets::UnlitMaterial>();
        assets.getMaterial<Core::Assets::UnlitMaterial>( unlitHandle )->setColor( { 1.0f, 0.2f, 0.2f } );

        auto unlitHandle2 = assets.material( "UnlitBlue" ).create<Core::Assets::UnlitMaterial>();
        assets.getMaterial<Core::Assets::UnlitMaterial>( unlitHandle2 )->setColor( { 0.2f, 0.4f, 1.0f } );

        auto debugHandle = assets.material( "DebugMtl" ).create<Core::Assets::DebugMaterial>();

        // 2. PROCEDURAL (GRID 10x10x10)
        // -------------------------------------------------------------------------

        std::vector<Core::Assets::MeshHandle>     meshPool = { cubeHandle, sphereHandle };
        std::vector<Core::Assets::MaterialHandle> matPool  = { unlitHandle, unlitHandle2, debugHandle };

        std::random_device                    rd;
        std::mt19937                          gen( rd() );
        std::uniform_int_distribution<>       meshDist( 0, (int)meshPool.size() - 1 );
        std::uniform_int_distribution<>       matDist( 0, (int)matPool.size() - 1 );
        std::uniform_real_distribution<float> scaleDist( 0.5f, 1.2f );
        std::uniform_real_distribution<float> rotDist( 0.0f, 360.0f );

        int   gridSize = 30; // 10x10x10 = 1000 objetos
        float spacing  = 3.0f;
        float offset   = ( gridSize * spacing ) * 0.5f;

        uint counter = 0;
        for ( int x = 0; x < gridSize; ++x )
        {
            for ( int y = 0; y < gridSize; ++y )
            {
                for ( int z = 0; z < gridSize; ++z )
                {
                    // Crear Entidad
                    auto entity = scene.createEntity( "GridObj" + counter );
                    counter++;

                    // Elegir Mesh y Material al azar
                    auto selectedMesh = meshPool[meshDist( gen )];
                    auto selectedMat  = matPool[matDist( gen )];

                    entity.addComponent<Core::Scene::MeshComponent>( selectedMesh, selectedMat );

                    float posX = ( x * spacing ) - offset;
                    float posY = ( y * spacing ) - offset;
                    float posZ = ( z * spacing ) - offset;

                    auto& transform       = entity.getComponent<Core::Scene::TransformComponent>();
                    transform.translation = { posX, posY, posZ };

                    transform.rotate( { Math::radians( rotDist( gen ) ),
                                        Math::radians( rotDist( gen ) ),
                                        0.0f } );

                    float s         = scaleDist( gen );
                    transform.scale = { s, s, s };
                }
            }
        }

        // 3. SCENE SETUP
        // -------------------------------------------------------------------------
        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();
        cameraEntity.getComponent<Core::Scene::TransformComponent>().position( { 0.0f, 0.0f, -50.0f } );
        cameraEntity.getComponent<Core::Scene::TransformComponent>().lookAt( { 0.0f, 0.0f, 0.0f } );

        // //Evento simple de borrado (opcional, borra el ultimo creado si pulsas W)
        // auto eraseEvent = wnd.onKey().subscribe( [&]( const Event::KeyEvent& e ) {
        //     if ( e.keyCode == Event::KeyCode::W && e.pressed )
        //         // Nota: destroyEntity necesita una entidad válida,
        //         // aquí solo es ejemplo, mejor no borrar nada en el stress test
        //         // Logger::info( "Key Pressed" );
        // } );

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
                swprintf_s( buffer, 100, L"FPS: %.2f | Objects: %d\n", fps, gridSize * gridSize * gridSize );
                OutputDebugStringW( buffer );

                frameCounter   = 0;
                elapsedSeconds = 0.0;
            }

            // Animación simple: Rotate Camera
            float time   = std::chrono::duration<float>( t1 - startTime ).count();
            float radius = 50.0f;
            float camX   = sin( time * 0.2f ) * radius;
            float camZ   = cos( time * 0.2f ) * radius;

            auto& camTrans = cameraEntity.getComponent<Core::Scene::TransformComponent>();
            camTrans.position( { camX, radius * 0.5f, camZ } );
            camTrans.lookAt( { 0.0f, 0.0f, 0.0f } );

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