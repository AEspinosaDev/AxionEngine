#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/StandardPBRMaterial.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/Rasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"
#include <vector>

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreInitializationTest.log" );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Axion PBR Showcase" } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "TestScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.common.name             = "MyRasterizer";
        rastDesc.useGPUCulling           = true;
        rastDesc.common.selectedDeviceID = 0;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // =================================================================================
        // 1. ASSET LOADING (GEOMETRY & Textures)
        // =================================================================================
        auto cubeHandle   = assets.mesh( "Cube" ).createCube();
        auto sphereHandle = assets.mesh( "Sphere" ).createSphere();
        auto dragonHandle = assets.mesh( "Dragon" ).import( AXION_MESH_DIR "/dragon.obj" );
        auto ajaxHandle   = assets.mesh( "Ajax" ).import( AXION_MESH_DIR "/ajax.obj" );

        auto albedoTexHandle  = assets.texture( "AxionTexture" ).import( AXION_TEXTURE_DIR "/Axion.png" );
        auto albedoTexHandle2 = assets.texture( "PlanetTexture" ).import( AXION_TEXTURE_DIR "/Jupiter.jpg", Core::Assets::TextureImportAsGamma | Core::Assets::TextureImportForce4Channels | Core::Assets::TextureImportFlipVertically );

        // =================================================================================
        // 2. PBR MATERIAL CREATION (PHYSICAL VARIETY)
        // =================================================================================

        // Material 1: Gold (For the Dragon) - Metallic and smooth
        auto matGoldH = assets.material( "Gold" ).create<Core::Assets::StandardPBRMaterial>();
        auto matGold  = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matGoldH );
        matGold->setAlbedo( { 1.0f, 0.76f, 0.33f } ); // Characteristic Gold Color
        matGold->setMetallic( 1.0f );
        matGold->setRoughness( 0.2f ); // Polished

        // Material 2: Shiny Red Plastic (For the Sphere) - Dielectric
        auto matRedPlasticH = assets.material( "Jupiter" ).create<Core::Assets::StandardPBRMaterial>();
        auto matRed         = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matRedPlasticH );
        matRed->setAlbedoTexture( albedoTexHandle2 );
        matRed->setMetallic( 0.0f );  // Plastic/Dielectric
        matRed->setRoughness( 0.8f ); // Very glossy (Sharp reflections)

        
        auto matChromeH = assets.material( "ChromeBlue" ).create<Core::Assets::StandardPBRMaterial>();
        auto matChrome  = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matChromeH );
        matChrome->setAlbedo( { 0.3f, 0.5f, 1.0f } ); // Light Blue Tint
        matChrome->setMetallic( 0.0f );
        matChrome->setRoughness( 0.3f ); // Almost mirror

        // Material 4: Grey Rubber/Matte (For the Cube) - Rough
        auto matRubberH = assets.material( "Rubber" ).create<Core::Assets::StandardPBRMaterial>();
        auto matRubber  = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matRubberH );
        matRubber->setAlbedoTexture( albedoTexHandle );
        matRubber->setMetallic( 0.0f );
        matRubber->setRoughness( 0.8f ); // Very matte, scatters light

        // =================================================================================
        // 3. SCENE SETUP (OBJECTS)
        // =================================================================================

        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();
        cameraEntity.getComponent<Core::Scene::TransformComponent>().position( { 0.0f, 0.0f, 5.0f } );
        cameraEntity.getComponent<Core::Scene::TransformComponent>().lookAt( { 0.0f, 0.0f, 0.0f } );
        cameraEntity.getComponent<Core::Scene::CameraComponent>().exposureCompensation = 5.0f;

        // Dragon (Top Left) -> GOLD
        auto dragonEntity = scene.createEntity( "Dragon" );
        dragonEntity.addComponent<Core::Scene::MeshComponent>( dragonHandle, matGoldH );
        dragonEntity.getComponent<Core::Scene::TransformComponent>().translation = { -1.5f, 1.5f, 0.0f };
        dragonEntity.getComponent<Core::Scene::TransformComponent>().scale       = { 2.0f, 2.0f, 2.0f };

        // Sphere (Top Right) -> RED PLASTIC
        auto sphereEntity = scene.createEntity( "Sphere" );
        sphereEntity.addComponent<Core::Scene::MeshComponent>( sphereHandle, matRedPlasticH );
        sphereEntity.getComponent<Core::Scene::TransformComponent>().translation = { 1.5f, 1.5f, 0.0f };
        sphereEntity.getComponent<Core::Scene::TransformComponent>().scale       = { 0.8f, 0.8f, 0.8f };

        // Ajax (Bottom Right) -> BLUE CHROME
        auto ajaxEntity = scene.createEntity( "Ajax" );
        ajaxEntity.addComponent<Core::Scene::MeshComponent>( ajaxHandle, matChromeH );
        ajaxEntity.getComponent<Core::Scene::TransformComponent>().translation = { 1.5f, -1.5f, 0.0f };
        ajaxEntity.getComponent<Core::Scene::TransformComponent>().scale       = { 2.0f, 2.0f, 2.0f };

        // Cube (Bottom Left) -> GREY RUBBER
        auto cubeEntity = scene.createEntity( "Cube" );
        cubeEntity.addComponent<Core::Scene::MeshComponent>( cubeHandle, matRubberH );
        cubeEntity.getComponent<Core::Scene::TransformComponent>().translation = { -1.5f, -1.5f, 0.0f };
        cubeEntity.getComponent<Core::Scene::TransformComponent>().scale       = { 1.25f, 1.25f, 1.25f };

        // =================================================================================
        // 4. LIGHTING SETUP (ATMOSPHERE + LIGHTS)
        // =================================================================================

        // A. Global Constant Ambient
        auto envEntity = scene.createEntity( "GlobalEnvironment" );
        envEntity.addComponent<Core::Scene::EnvironmentComponent>();
        auto& env       = envEntity.getComponent<Core::Scene::EnvironmentComponent>();
        env.active      = true;
        env.type        = Core::Scene::EnvironmentComponent::Type::Global;
        env.skyType     = Core::Scene::EnvironmentComponent::SkyType::Constant;
        env.skyColor    = { 0.1f, 0.1f, 0.5f };
        env.groundColor = { 0.2f, 0.2f, 0.2f };
        env.intensity   = 1.0f;

        // B. Directional Light (Moon / Key Light)
        auto sunEntity = scene.createEntity( "MoonLight" );
        sunEntity.addComponent<Core::Scene::LightComponent>();
        auto& sun          = sunEntity.getComponent<Core::Scene::LightComponent>();
        sun.type           = Core::Scene::LightComponent::Type::Directional;
        sun.intensity      = 50.0f;                // Lux (adjusted to avoid burnout without tonemapping)
        sun.color          = { 0.8f, 0.9f, 1.0f }; // Cold Blueish White
        sun.useTemperature = true;
        sun.temperature    = 8000.0f;
        sunEntity.getComponent<Core::Scene::TransformComponent>().lookAt( { 1.0f, -1.0f, -0.5f } ); // Coming from top-left

        // Main Loop
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

            // Delta time in seconds (critical for frame-rate independent rotation)
            float dt = deltaTime.count() * 1e-9f;

            elapsedSeconds += dt;
            if ( elapsedSeconds > 1.0 )
            {
                wchar_t buffer[100];
                double  fps = frameCounter / elapsedSeconds;
                swprintf_s( buffer, 100, L"Axion Engine | FPS: %.2f\n", fps );
                OutputDebugStringW( buffer );
                frameCounter   = 0;
                elapsedSeconds = 0.0;
            }

            float rotSpeed = 0.5f; // Radians per second
            float step     = rotSpeed * dt;

            dragonEntity.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, step, 0.0f } );
            ajaxEntity.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, -step, 0.0f } );
            sphereEntity.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, -step, 0.0f } );

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