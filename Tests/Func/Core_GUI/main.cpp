#pragma once
#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/StandardPBRMaterial.h"
#include "Axion/Core/GUI/GUI.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/IRasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"

#include "Axion/Common/Containers/SmallVector.h"

USING_AXION_NAMESPACE

int main( /*int argc, char* argv[]*/ ) {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreGUITest.log" );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Core GUI Test" } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "TestScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.common.name             = "MyRasterizer";
        rastDesc.common.selectedDeviceID = 0;
        rastDesc.common.flags |= Core::Render::RendererEnableGUI | Core::Render::RendererEnableFXAA;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // =================================================================================
        // 1. ASSET LOADING (GEOMETRY & Textures)
        // =================================================================================
        auto sphereHandle     = assets.mesh( "Sphere" ).createSphere();
        auto albedoTexHandle2 = assets.texture( "PlanetTexture" ).import( AXION_TEXTURE_DIR "/Jupiter.jpg", Core::Assets::TextureImportAsGamma | Core::Assets::TextureImportForce4Channels | Core::Assets::TextureImportFlipVertically );

        // =================================================================================
        // 2. PBR MATERIAL CREATION (PHYSICAL VARIETY)
        // =================================================================================

        auto matRedPlasticH = assets.material( "Jupiter" ).create<Core::Assets::StandardPBRMaterial>();
        auto matRed         = assets.getMaterial<Core::Assets::StandardPBRMaterial>( matRedPlasticH );
        matRed->setAlbedoTexture( albedoTexHandle2 );
        matRed->setMetallic( 0.0f );
        matRed->setRoughness( 0.8f );

        // =================================================================================
        // 3. SCENE SETUP (OBJECTS)
        // =================================================================================

        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();
        cameraEntity.getComponent<Core::Scene::TransformComponent>().position( { 0.0f, 0.0f, 5.0f } );
        cameraEntity.getComponent<Core::Scene::TransformComponent>().lookAt( { 0.0f, 0.0f, 0.0f } );
        cameraEntity.getComponent<Core::Scene::CameraComponent>().setExposureCompensation( 5.0f );

        auto sphereEntity = scene.createEntity( "Sphere" );
        sphereEntity.addComponent<Core::Scene::MeshComponent>( sphereHandle, matRedPlasticH );
        sphereEntity.getComponent<Core::Scene::TransformComponent>().scaleUniform( 2.0f );

        // =================================================================================
        // 4. LIGHTING SETUP (ATMOSPHERE + LIGHTS)
        // =================================================================================

        // A. Global Constant Ambient
        auto envEntity = scene.createEntity( "GlobalEnvironment" );
        envEntity.addComponent<Core::Scene::EnvironmentComponent>();
        auto& env = envEntity.getComponent<Core::Scene::EnvironmentComponent>();
        env.setActive( true );
        env.setSkyType( Core::Scene::EnvironmentComponent::SkyType::Constant );
        env.setSkyColor( { 0.1f, 0.1f, 0.5f } );
        env.setGroundColor( { 0.2f, 0.2f, 0.2f } );
        env.setIntensity( 1.0f );

        // B. Directional Light (Moon / Key Light)
        auto sunEntity = scene.createEntity( "MoonLight" );
        sunEntity.addComponent<Core::Scene::LightComponent>();
        auto& sun = sunEntity.getComponent<Core::Scene::LightComponent>();
        sun.setType( Core::Scene::LightComponent::Type::Directional );
        sun.setIntensity( 50.0f );            // Lux (adjusted to avoid burnout without tonemapping)
        sun.setColor( { 0.8f, 0.9f, 1.0f } ); // Cold Blueish White
        sun.setUseTemperature( true );
        sun.setTemperature( 8000.0f );
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

            float rotSpeed = 0.1f; // Radians per second
            float step     = rotSpeed * dt;

            sphereEntity.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, -step, 0.0f } );

            wnd.update();

            rasterizer->newGuiFrame();

            // GUI LOGIC HERE
            {
                ImGui::ShowDemoWindow();
            }

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