#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/StandardPBRMaterial.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/IRasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"
#include <map>
#include <random>
#include <vector>

USING_AXION_NAMESPACE

// Helper para gestionar el input de forma más cómoda
struct InputState {
    std::map<Event::KeyCode, bool> keys;
    bool                           rightMousePressed = false;
    float                          lastMouseX        = 0.0f;
    float                          lastMouseY        = 0.0f;
    float                          yaw               = 0.0f;
    float                          pitch             = 0.0f;
};

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreInitializationTest.log" );
#endif

        Core::Platform::Window     wnd( {
                .platformType = Graphics::PlatformType::Win32,
                .name         = "100K Instances - Free Camera Test",
        } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "TestScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.common.name             = "TestRasterizer";
        rastDesc.common.selectedDeviceID = 0;
        rastDesc.common.flags |= Core::Render::RendererEnableFXAA;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // 1. ASSETS & MATERIALS
        auto cubeHandle   = assets.mesh( "Cube" ).createCube();
        auto sphereHandle = assets.mesh( "Sphere" ).createSphere();

        auto matGoldH = assets.material( "Gold" ).create<Core::Assets::StandardPBRMaterial>();
        auto matGold  = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matGoldH );
        matGold->setAlbedo( { 1.0f, 0.76f, 0.33f } );
        matGold->setMetallic( 1.0f );
        matGold->setRoughness( 0.3f );

        auto matRedH = assets.material( "RedPlastic" ).create<Core::Assets::StandardPBRMaterial>();
        auto matRed  = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matRedH );
        matRed->setAlbedo( { 0.8f, 0.05f, 0.05f } );
        matRed->setMetallic( 0.0f );
        matRed->setRoughness( 0.2f );

        auto matSilverH = assets.material( "BrushedSteel" ).create<Core::Assets::StandardPBRMaterial>();
        auto matSilver  = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matSilverH );
        matSilver->setAlbedo( { 0.6f, 0.65f, 0.7f } );
        matSilver->setMetallic( 1.0f );
        matSilver->setRoughness( 0.45f );

        // 2. LIGHTING
        auto envEntity = scene.createEntity( "GlobalVolume" );
        envEntity.addComponent<Core::Scene::EnvironmentComponent>();
        auto& env = envEntity.getComponent<Core::Scene::EnvironmentComponent>();
        env.setSkyColor( { 0.5f, 0.7f, 1.0f } );
        env.setGroundColor( { 0.2f, 0.2f, 0.25f } );
        env.setIntensity( 1.0f );

        auto sunEntity = scene.createEntity( "Sun" );
        sunEntity.addComponent<Core::Scene::LightComponent>();
        auto& sunComp = sunEntity.getComponent<Core::Scene::LightComponent>();
        sunComp.setType( Core::Scene::LightComponent::Type::Directional );
        sunComp.setIntensity( 10.0f );
        sunComp.setColor( { 1.0f, 0.95f, 0.9f } );
        sunComp.setUseTemperature( false );

        auto& sunTrans = sunEntity.getComponent<Core::Scene::TransformComponent>();
        sunTrans.lookAt( { 1.0f, -1.0f, 0.5f } );

        // 3. GENERACIÓN PROCEDURAL
        std::vector<Core::Assets::MeshHandle>     meshPool = { cubeHandle, sphereHandle };
        std::vector<Core::Assets::MaterialHandle> matPool  = { matGoldH, matRedH, matSilverH };

        std::random_device                    rd;
        std::mt19937                          gen( rd() );
        std::uniform_int_distribution<>       meshDist( 0, (int)meshPool.size() - 1 );
        std::uniform_int_distribution<>       matDist( 0, (int)matPool.size() - 1 );
        std::uniform_real_distribution<float> scaleDist( 0.5f, 1.2f );
        std::uniform_real_distribution<float> rotDist( 0.0f, 360.0f );

        int   gridSize = 50;
        float spacing  = 3.0f;
        float offset   = ( gridSize * spacing ) * 0.5f;

        u32 counter = 0;
        for ( int x = 0; x < gridSize; ++x )
        {
            for ( int y = 0; y < gridSize; ++y )
            {
                for ( int z = 0; z < gridSize; ++z )
                {
                    auto entity = scene.createEntity( "GridObj" + std::to_string( counter++ ) );

                    entity.addComponent<Core::Scene::MeshComponent>(
                        meshPool[meshDist( gen )],
                        matPool[matDist( gen )] );

                    auto& transform = entity.getComponent<Core::Scene::TransformComponent>();
                    transform.setTranslation( { ( x * spacing ) - offset,
                                                ( y * spacing ) - offset,
                                                ( z * spacing ) - offset } );

                    transform.rotate( { Math::radians( rotDist( gen ) ), Math::radians( rotDist( gen ) ), 0.0f } );
                    float s = scaleDist( gen );
                    transform.setScale( { s, s, s } );
                }
            }
        }

        // 4. CÁMARA & INPUT SETUP
        // -------------------------------------------------------------------------
        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();

        auto& camTrans = cameraEntity.getComponent<Core::Scene::TransformComponent>();
        camTrans.position( { 0.0f, 0.0f, 100.0f } );
        camTrans.lookAt( { 0.0f, 0.0f, 0.0f } );

        cameraEntity.getComponent<Core::Scene::CameraComponent>().setExposureCompensation( 8.5f );

        InputState input;

        auto keySub = wnd.onKey().subscribe( [&]( const Event::KeyEvent& e ) {
            input.keys[e.keyCode] = e.pressed;
        } );

        auto mouseBtnSub = wnd.onMouseButton().subscribe( [&]( const Event::MouseButtonEvent& e ) {
            if ( e.button == 1 )
            {
                input.rightMousePressed = e.pressed;
                if ( e.pressed )
                {
                } else
                {
                }
            }
        } );

        // Ratón (Movimiento)
        auto mouseMoveSub = wnd.onMouseMove().subscribe( [&]( const Event::MouseMoveEvent& e ) {
            if ( input.rightMousePressed )
            {
                float sensitivity = 0.002f;
                float deltaX      = e.x - input.lastMouseX;
                float deltaY      = e.y - input.lastMouseY;

                input.yaw -= deltaX * sensitivity;
                input.pitch -= deltaY * sensitivity;

                input.pitch = std::max( -1.5f, std::min( 1.5f, input.pitch ) );
            }
            input.lastMouseX = (float)e.x;
            input.lastMouseY = (float)e.y;
        } );

        // BUCLE PRINCIPAL
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

            float dt = std::chrono::duration<float>( deltaTime ).count();

            elapsedSeconds += dt;
            if ( elapsedSeconds > 1.0 )
            {
                double fps = frameCounter / elapsedSeconds;

                wchar_t buffer[256];
                swprintf_s( buffer, 256, L"Axion Engine | Objects: %d | FPS: %.2f | GPU: MDI Active", gridSize * gridSize * gridSize, fps );

                OutputDebugStringW( buffer );
                OutputDebugStringW( L"\n" );

                frameCounter   = 0;
                elapsedSeconds = 0.0;
            }

            float speed = 20.0f * dt; // Unidades por segundo
            if ( input.keys[Event::KeyCode::Shift] )
                speed *= 4.0f; // Turbo con Shift

            Math::Vec3 forward = camTrans.forward();
            Math::Vec3 right   = camTrans.right();
            Math::Vec3 up      = { 0.0f, 1.0f, 0.0f }; // Global UP para movimiento más natural

            Math::Vec3 movement = { 0.0f, 0.0f, 0.0f };

            if ( input.keys[Event::KeyCode::W] )
                movement += forward;
            if ( input.keys[Event::KeyCode::S] )
                movement -= forward;
            if ( input.keys[Event::KeyCode::D] )
                movement += right;
            if ( input.keys[Event::KeyCode::A] )
                movement -= right;
            if ( input.keys[Event::KeyCode::Q] )
                movement += up; // Subir
            if ( input.keys[Event::KeyCode::E] )
                movement -= up; // Bajar

            if ( Math::length( movement ) > 0.0f )
            {
                movement = Math::normalize( movement ) * speed;
                camTrans.translate( movement );
            }

            camTrans.rotate( { input.pitch, input.yaw, 0.0f } ); // Roll siempre 0

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