#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/PhongMaterial.h"
#include "Axion/Core/Assets/Materials/StandardPBRMaterial.h"
#include "Axion/Core/Assets/Materials/UnlitMaterial.h"
#include "Axion/Core/GUI/GUI.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/IRasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

USING_AXION_NAMESPACE

// Material binning test: the asteroid belt of the particle system sample, made of three overlapping particle systems,
// each one with a different material archetype (one resolve bin per system). The menu switches between the binned and the
// naive resolve, and the GPU time of every render graph pass is averaged and reported once per second.

namespace {

constexpr u32   PARTICLES_PER_SYSTEM  = 800000;
constexpr float PLANET_RADIUS         = 3.0f;  // createSphere has radius 1
constexpr float PLANET_ROTATION_SPEED = 0.01f; // Radians per second
constexpr float REPORT_PERIOD         = 1.0f;  // Seconds averaged per timing report

constexpr float CAMERA_MOVE_SPEED  = 0.5f;   // Units per second
constexpr float CAMERA_FAST_FACTOR = 2.0f;   // While holding shift
constexpr float CAMERA_LOOK_SPEED  = 0.003f; // Radians per pixel
constexpr float CAMERA_MAX_PITCH   = 1.55f;  // Radians. Just below straight up or down

/// @brief Where the camera is and where it looks (radians).
struct CameraPose {
    Math::Vec3 position;
    float      yaw;
    float      pitch;
};

// Same pose as the particle system sample. To change it: place the camera, press Copy pose in the menu and paste it here
constexpr CameraPose INITIAL_CAMERA_POSE = { { 0.0f, 0.04f, 13.7f }, -0.185f, -0.045f };

/**
 * @brief Fly camera: WASD moves it (Q/E down and up, shift faster) and dragging with the right mouse button looks around.
 * The input comes from ImGui, which knows when the menu owns it.
 */
class FlyCamera
{
public:
    /**
     * @param pose Initial pose.
     * @param transform Transform of the camera: it takes that pose.
     */
    FlyCamera( const CameraPose& pose, Core::Scene::TransformComponent& transform )
        : _yaw( pose.yaw )
        , _pitch( pose.pitch ) {
        transform.position( pose.position );
        transform.setRotation( getRotation() );
    }

    /**
     * @brief Applies the input of this frame. Call it between newGuiFrame and render.
     * @param transform Transform of the camera.
     * @param dt Delta time in seconds.
     */
    void update( Core::Scene::TransformComponent& transform, float dt ) {
        const ImGuiIO& io = ImGui::GetIO();

        if ( !io.WantCaptureMouse && ImGui::IsMouseDown( ImGuiMouseButton_Right ) )
        {
            _yaw -= io.MouseDelta.x * CAMERA_LOOK_SPEED;
            _pitch = Math::clamp( _pitch - io.MouseDelta.y * CAMERA_LOOK_SPEED, -CAMERA_MAX_PITCH, CAMERA_MAX_PITCH );
            transform.setRotation( getRotation() );
        }

        if ( io.WantCaptureKeyboard )
            return;

        const auto axis = []( ImGuiKey positive, ImGuiKey negative ) {
            return ( ImGui::IsKeyDown( positive ) ? 1.0f : 0.0f ) - ( ImGui::IsKeyDown( negative ) ? 1.0f : 0.0f );
        };

        const Math::Vec3 direction = transform.forward() * axis( ImGuiKey_W, ImGuiKey_S ) +
                                     transform.right() * axis( ImGuiKey_D, ImGuiKey_A ) +
                                     transform.up() * axis( ImGuiKey_E, ImGuiKey_Q );

        const float speed = CAMERA_MOVE_SPEED * ( io.KeyShift ? CAMERA_FAST_FACTOR : 1.0f );
        transform.translate( direction * speed * dt );
    }

    /// @param transform Transform of the camera.
    CameraPose getPose( const Core::Scene::TransformComponent& transform ) const {
        return { transform.getTranslation(), _yaw, _pitch };
    }

private:
    Math::Quat getRotation() const {
        return Math::Quat( Math::Vec3( _pitch, _yaw, 0.0f ) );
    }

    float _yaw   = 0.0f;
    float _pitch = 0.0f;
};

/// @brief GPU time of each render graph pass, averaged over a report period.
class PassTimingAverager
{
public:
    struct PassAverage {
        std::string name;
        double      milliseconds;
    };

    /// @brief Adds the timings of the last finished frame.
    void accumulate( const STLW::Vector<Graphics::GPUPassTiming>& timings ) {
        for ( const auto& timing : timings )
        {
            Entry& entry = findOrAdd( timing.name.cstr() );
            entry.sum += timing.milliseconds;
            entry.samples++;
        }
    }

    /// @brief Averages since the last flush, in pass order. Starts a new period.
    std::vector<PassAverage> flush() {
        std::vector<PassAverage> averages;
        for ( auto& entry : _entries )
        {
            if ( entry.samples > 0 )
                averages.push_back( { entry.name, entry.sum / entry.samples } );
            entry.sum     = 0.0;
            entry.samples = 0;
        }
        return averages;
    }

private:
    struct Entry {
        std::string name;
        double      sum     = 0.0;
        u32         samples = 0;
    };

    Entry& findOrAdd( const char* name ) {
        for ( auto& entry : _entries )
        {
            if ( entry.name == name )
                return entry;
        }
        return _entries.emplace_back( Entry { name } );
    }

    std::vector<Entry> _entries;
};

/// @brief Sends a line to the debugger output (any configuration) and to the engine log (debug builds only).
void report( const char* line ) {
    OutputDebugStringA( ( std::string( line ) + "\n" ).c_str() ); // A single message: the debugger shows one line
    AXION_LOG_INFO( Logger::Module::Core, "{}", line );
}

/// @brief A particle system and the label of its material in the menu.
struct MaterialSystem {
    const char*         label;
    Core::Scene::Entity entity;
};

/// @brief True if the rasterizer resolves with one full screen dispatch per material bundle.
bool usesNaiveResolve( const Core::Render::IRasterizer& rasterizer ) {
    return ( rasterizer.getSettings().common.flags & Core::Render::RendererUseNaiveResolve ) != Core::Render::RendererNone;
}

/**
 * @brief Resolve mode, system visibility, last timing report and pose of the camera.
 * @param rasterizer Its resolve mode is switched by the menu.
 * @param systems Particle systems toggled by the menu.
 * @param averages Last report, shown as a table.
 * @param cameraPose Pose shown by the menu, ready to copy.
 */
void drawMenu( Core::Render::IRasterizer& rasterizer, std::vector<MaterialSystem>& systems, const std::vector<PassTimingAverager::PassAverage>& averages, const CameraPose& cameraPose ) {
    ImGui::Begin( "Material Binning" );
    ImGui::Text( "%.1f FPS", ImGui::GetIO().Framerate );

    // Read every frame by the rasterizer: both resolves are always compiled, so the switch is immediate
    bool naiveResolve = usesNaiveResolve( rasterizer );
    if ( ImGui::Checkbox( "Naive resolve", &naiveResolve ) )
    {
        auto settings = rasterizer.getSettings();
        if ( naiveResolve )
            settings.common.flags |= Core::Render::RendererUseNaiveResolve;
        else
            settings.common.flags = settings.common.flags & ~Core::Render::RendererUseNaiveResolve;
        rasterizer.setSettings( settings );
    }

    ImGui::SeparatorText( "Particle systems" );
    for ( auto& materialSystem : systems )
    {
        auto& system  = materialSystem.entity.getComponent<Core::Scene::ParticleSystemComponent>();
        bool  visible = system.isVisible();
        if ( ImGui::Checkbox( materialSystem.label, &visible ) )
            system.setVisible( visible );
    }

    ImGui::SeparatorText( "GPU time per pass (1 s average)" );
    for ( const auto& average : averages )
        ImGui::Text( "%-28s %8.3f ms", average.name.c_str(), average.milliseconds );

    ImGui::SeparatorText( "Camera" );
    ImGui::TextDisabled( "WASD, Q/E, shift, right mouse to look" );

    // Ready to paste in INITIAL_CAMERA_POSE
    char pose[128];
    std::snprintf( pose, sizeof( pose ), "{ { %.4ff, %.4ff, %.4ff }, %.4ff, %.4ff }", cameraPose.position.x, cameraPose.position.y, cameraPose.position.z, cameraPose.yaw, cameraPose.pitch );
    ImGui::TextWrapped( "%s", pose );
    if ( ImGui::Button( "Copy pose" ) )
        ImGui::SetClipboardText( pose );

    ImGui::End();
}

} // namespace

/// @brief Arguments: --naive-resolve starts with the naive resolve (the menu can switch it later)
int main( int argc, char** argv ) {

    try
    {
        const bool startNaive = argc > 1 && std::string( argv[1] ) == "--naive-resolve";

#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreMaterialBinningTest.log" );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Material Binning Test" } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "MaterialBinningScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.common.name             = "MaterialBinningRasterizer";
        rastDesc.common.selectedDeviceID = 0;
        rastDesc.common.flags |= Core::Render::RendererEnableGUI | Core::Render::RendererEnableFXAA | Core::Render::RendererEnableGPUTimeQueries;
        if ( startNaive )
            rastDesc.common.flags |= Core::Render::RendererUseNaiveResolve;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();
        auto& rasterizerSettings = static_cast<Core::Render::IRasterizer&>( *rasterizer ); // The menu switches its resolve

        // =================================================================================
        // 1. ASSET LOADING (GEOMETRY & Textures)
        // =================================================================================
        auto planetMeshHandle   = assets.mesh( "Planet" ).createSphere( 128 );
        auto sphereMeshHandle   = assets.mesh( "ParticleSphere" ).createSphere( 12 );
        auto cubeMeshHandle     = assets.mesh( "ParticleCube" ).createCube();
        auto asteroidMeshHandle = assets.mesh( "Asteroid" ).import( AXION_MESH_DIR "/asteroid.obj" );
        auto planetTexHandle    = assets.texture( "PlanetTexture" ).import( AXION_TEXTURE_DIR "/Jupiter.jpg", Core::Assets::TextureImportAsGamma | Core::Assets::TextureImportForce4Channels | Core::Assets::TextureImportFlipVertically );
        auto asteroidTexHandle  = assets.texture( "AsteroidTexture" ).import( AXION_TEXTURE_DIR "/asteroid.png" ); // No flip: the OBJ loader already flips its V

        // =================================================================================
        // 2. MATERIALS: one archetype per particle system, so every system is a resolve bin
        // =================================================================================
        auto planetMatHandle = assets.material( "Jupiter" ).create<Core::Assets::StandardPBRMaterial>();
        auto planetMat       = assets.getMaterial<Core::Assets::StandardPBRMaterial>( planetMatHandle );
        planetMat->setAlbedoTexture( planetTexHandle );
        planetMat->setMetallic( 0.0f );
        planetMat->setRoughness( 0.8f );

        auto goldMatHandle = assets.material( "GoldPBR" ).create<Core::Assets::StandardPBRMaterial>();
        auto goldMat       = assets.getMaterial<Core::Assets::StandardPBRMaterial>( goldMatHandle );
        goldMat->setAlbedo( { 1.0f, 0.71f, 0.29f } );
        goldMat->setMetallic( 1.0f );
        goldMat->setRoughness( 0.35f );

        auto neonMatHandle = assets.material( "NeonUnlit" ).create<Core::Assets::UnlitMaterial>();
        auto neonMat       = assets.getMaterial<Core::Assets::UnlitMaterial>( neonMatHandle );
        neonMat->setColor( { 0.1f, 0.9f, 1.0f } );
        neonMat->setEmissivePower( 1.0f );

        auto rockMatHandle = assets.material( "RockPhong" ).create<Core::Assets::PhongMaterial>();
        auto rockMat       = assets.getMaterial<Core::Assets::PhongMaterial>( rockMatHandle );
        rockMat->setDiffuseTexture( asteroidTexHandle );
        rockMat->setDiffuse( { 1.0f, 1.0f, 1.0f } );
        rockMat->setSpecular( { 0.4f, 0.4f, 0.4f } );
        rockMat->setShininess( 48.0f );

        // =================================================================================
        // 3. SCENE SETUP (OBJECTS)
        // =================================================================================
        auto cameraEntity = scene.createEntity( "MainCamera" );
        cameraEntity.addComponent<Core::Scene::CameraComponent>();
        cameraEntity.getComponent<Core::Scene::CameraComponent>().setExposureCompensation( 5.0f );
        FlyCamera flyCamera( INITIAL_CAMERA_POSE, cameraEntity.getComponent<Core::Scene::TransformComponent>() );

        auto planetEntity = scene.createEntity( "Planet" );
        planetEntity.addComponent<Core::Scene::MeshComponent>( planetMeshHandle, planetMatHandle );
        planetEntity.getComponent<Core::Scene::TransformComponent>().scaleUniform( PLANET_RADIUS );

        // The belt of the particle system sample, split in three overlapping systems. Slightly different extent, size and speed
        // so each one shows up, and the bins interleave all over the belt
        struct SystemDesc {
            const char*                  label;
            Core::Assets::MeshHandle     mesh;
            Core::Assets::MaterialHandle material;
            Math::Vec3                   extent; // Inner radius, thickness, outer radius
            float                        size;   // Mesh units to world: asteroid radius ~16, sphere 1, cube 0.5 (half side)
            float                        speed;
            u32                          seed;
        };
        const SystemDesc systemDescs[] = {
            { "Phong asteroids", asteroidMeshHandle, rockMatHandle, { 5.0f, 0.2f, 14.0f }, 0.00019f, 0.002f, 1 }, // The sample belt
            { "PBR spheres", sphereMeshHandle, goldMatHandle, { 5.5f, 0.25f, 13.5f }, 0.0020f, 0.0025f, 2 },
            { "Unlit cubes", cubeMeshHandle, neonMatHandle, { 6.0f, 0.15f, 13.0f }, 0.003f, 0.0015f, 3 },
        };

        std::vector<MaterialSystem> systems;
        for ( const auto& desc : systemDescs )
        {
            auto entity = scene.createEntity( desc.label );
            entity.addComponent<Core::Scene::ParticleSystemComponent>( desc.mesh, desc.material, PARTICLES_PER_SYSTEM );

            auto& system = entity.getComponent<Core::Scene::ParticleSystemComponent>();
            system.setExtent( desc.extent );
            system.setSize( desc.size );
            system.setSpeed( desc.speed );
            system.setSeed( desc.seed );

            systems.push_back( { desc.label, entity } );
        }

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

        // B. Directional Light (Sun)
        auto sunEntity = scene.createEntity( "Sun" );
        sunEntity.addComponent<Core::Scene::LightComponent>();
        auto& sun = sunEntity.getComponent<Core::Scene::LightComponent>();
        sun.setType( Core::Scene::LightComponent::Type::Directional );
        sun.setIntensity( 50.0f ); // Lux (adjusted to avoid burnout without tonemapping)
        sun.setColor( { 0.8f, 0.9f, 1.0f } );
        sun.setUseTemperature( true );
        sun.setTemperature( 8000.0f );
        sunEntity.getComponent<Core::Scene::TransformComponent>().lookAt( { 1.0f, -1.0f, -0.5f } );

        // Main Loop
        PassTimingAverager                       timings;
        std::vector<PassTimingAverager::PassAverage> lastAverages;
        float                                    reportElapsed = 0.0f;
        u32                                      reportFrames  = 0;

        std::chrono::high_resolution_clock clock;
        auto                               t0 = clock.now();
        while ( !wnd.shouldClose() )
        {
            const auto t1 = clock.now();

            // Delta time in seconds (critical for frame-rate independent rotation)
            const float dt = std::chrono::duration<float>( t1 - t0 ).count();
            t0             = t1;

            planetEntity.getComponent<Core::Scene::TransformComponent>().rotate( { 0.0f, -PLANET_ROTATION_SPEED * dt, 0.0f } );

            wnd.update();

            rasterizer->newGuiFrame();
            flyCamera.update( cameraEntity.getComponent<Core::Scene::TransformComponent>(), dt );
            drawMenu( rasterizerSettings, systems, lastAverages, flyCamera.getPose( cameraEntity.getComponent<Core::Scene::TransformComponent>() ) );

            // The particles are simulated with this delta time: without it they wouldn't move
            rasterizer->render( scene, cameraEntity, dt );

            // Timing report: averages of the last period
            timings.accumulate( rasterizer->getGPUPassTimings() );
            reportElapsed += dt;
            reportFrames++;
            if ( reportElapsed >= REPORT_PERIOD )
            {
                lastAverages = timings.flush();

                char line[160];
                std::snprintf( line, sizeof( line ), "MaterialBinning | %s resolve | FPS: %.2f", usesNaiveResolve( rasterizerSettings ) ? "Naive" : "Binned", reportFrames / reportElapsed );
                report( line );
                for ( const auto& average : lastAverages )
                {
                    std::snprintf( line, sizeof( line ), "GPU %s: %.3f ms", average.name.c_str(), average.milliseconds );
                    report( line );
                }

                reportElapsed = 0.0f;
                reportFrames  = 0;
            }
        }
    } catch ( const std::exception& )
    {
        return EXIT_FAILURE;
    }
#ifdef AXION_DEBUG
    Axion::Logger::shutdown();
#endif

    return EXIT_SUCCESS;
}
