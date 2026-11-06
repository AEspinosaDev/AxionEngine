#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Core/Assets/Materials/StandardPBRMaterial.h"
#include "Axion/Core/GUI/GUI.h"
#include "Axion/Core/Platform/Window.h"
#include "Axion/Core/Render/IRasterizer.h"
#include "Axion/Core/Scene/Entity.h"
#include "Axion/Core/Scene/Scene.h"

#include <algorithm>
#include <cstdio>

USING_AXION_NAMESPACE

namespace {

constexpr u32   INITIAL_PARTICLE_COUNT = 2500000;
constexpr int   MIN_PARTICLE_COUNT     = 1000;
constexpr int   MAX_PARTICLE_COUNT     = 3000000;
constexpr float PLANET_RADIUS          = 3.0f;  // createSphere has radius 1
constexpr float PLANET_ROTATION_SPEED  = 0.01f; // Radians per second

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

// To change it: place the camera, press Copy pose in the menu and paste it here
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

/**
 * @brief Settings of the particle system and pose of the camera.
 * @param belt Particle system edited by the menu.
 * @param cameraPose Pose shown by the menu, ready to copy.
 */
void drawParticleSystemMenu( Core::Scene::ParticleSystemComponent& belt, const CameraPose& cameraPose ) {
    ImGui::Begin( "Particle System" );
    ImGui::Text( "%.1f FPS", ImGui::GetIO().Framerate );

    bool visible = belt.isVisible();
    if ( ImGui::Checkbox( "Visible", &visible ) )
        belt.setVisible( visible );

    bool updating = belt.isUpdating();
    if ( ImGui::Checkbox( "Update", &updating ) )
        belt.setUpdating( updating );

    float size = belt.getSize();
    if ( ImGui::SliderFloat( "Size", &size, 0.00001f, 0.05f, "%.5f", ImGuiSliderFlags_Logarithmic ) )
        belt.setSize( size );

    float speed = belt.getSpeed();
    if ( ImGui::SliderFloat( "Speed", &speed, 0.0f, 2.0f ) )
        belt.setSpeed( speed );

    // Only read when the particles are seeded: changing them seeds the particles again
    ImGui::SeparatorText( "Seeding" );

    // Every count creates buffers: applied when the slider is released, not per value while dragging
    static int count = (int)belt.getCount();
    ImGui::SliderInt( "Count", &count, MIN_PARTICLE_COUNT, MAX_PARTICLE_COUNT, "%d", ImGuiSliderFlags_Logarithmic );
    if ( ImGui::IsItemDeactivatedAfterEdit() )
        belt.setCount( (u32)count );

    float sizeVariation = belt.getSizeVariation();
    if ( ImGui::SliderFloat( "Size variation", &sizeVariation, 1.0f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic ) )
        belt.setSizeVariation( sizeVariation );

    Math::Vec3 extent = belt.getExtent();
    ImGui::SliderFloat( "Inner radius", &extent.x, PLANET_RADIUS, 30.0f );
    ImGui::SliderFloat( "Outer radius", &extent.z, extent.x, 40.0f );
    ImGui::SliderFloat( "Thickness", &extent.y, 0.0f, 5.0f );
    extent.z = std::max( extent.z, extent.x );
    belt.setExtent( extent );

    int seed = (int)belt.getSeed();
    if ( ImGui::InputInt( "Seed", &seed ) )
        belt.setSeed( (u32)seed );

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

int main() {

    try
    {
#ifdef AXION_DEBUG
        Logger::init( Logger::Level::Info, "CoreParticleSystem.log" );
#endif

        Core::Platform::Window     wnd( { .platformType = Graphics::PlatformType::Win32,
                                          .name         = "Axion Particle System" } );
        Core::Assets::AssetManager assets;
        Core::Scene::Scene         scene( "ParticleSystemScene", &assets );

        Core::Render::RasterizerSettings rastDesc {};
        rastDesc.common.name             = "MyRasterizer";
        rastDesc.common.selectedDeviceID = 0;
        rastDesc.common.flags |= Core::Render::RendererEnableGUI | Core::Render::RendererEnableFXAA;

        auto rasterizer = Core::Render::createRasterizer( &wnd, rastDesc );
        rasterizer->compileShaders();

        // =================================================================================
        // 1. ASSET LOADING (GEOMETRY & Textures)
        // =================================================================================
        auto planetMeshHandle   = assets.mesh( "Planet" ).createSphere( 128 );
        auto asteroidMeshHandle = assets.mesh( "Asteroid" ).import( AXION_MESH_DIR "/asteroid.obj" );
        auto planetTexHandle    = assets.texture( "PlanetTexture" ).import( AXION_TEXTURE_DIR "/Jupiter.jpg", Core::Assets::TextureImportAsGamma | Core::Assets::TextureImportForce4Channels | Core::Assets::TextureImportFlipVertically );
        auto asteroidTexHandle  = assets.texture( "AsteroidTexture" ).import( AXION_TEXTURE_DIR "/asteroid.png" ); // No flip: the OBJ loader already flips its V

        // =================================================================================
        // 2. PBR MATERIAL CREATION
        // =================================================================================
        auto planetMatHandle = assets.material( "Jupiter" ).create<Core::Assets::StandardPBRMaterial>();
        auto planetMat       = assets.getMaterial<Core::Assets::StandardPBRMaterial>( planetMatHandle );
        planetMat->setAlbedoTexture( planetTexHandle );
        planetMat->setMetallic( 0.0f );
        planetMat->setRoughness( 0.8f );

        auto asteroidMatHandle = assets.material( "Asteroid" ).create<Core::Assets::StandardPBRMaterial>();
        auto asteroidMat       = assets.getMaterial<Core::Assets::StandardPBRMaterial>( asteroidMatHandle );
        asteroidMat->setAlbedoTexture( asteroidTexHandle );
        asteroidMat->setMetallic( 0.1f );
        asteroidMat->setRoughness( 0.7f );

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

        auto beltEntity = scene.createEntity( "AsteroidBelt" );
        beltEntity.addComponent<Core::Scene::ParticleSystemComponent>( asteroidMeshHandle, asteroidMatHandle, INITIAL_PARTICLE_COUNT );
        auto& belt = beltEntity.getComponent<Core::Scene::ParticleSystemComponent>();
        belt.setExtent( { 5.0f, 0.2f, 14.0f } ); // Inner radius, thickness, outer radius
        belt.setSize( 0.00015f ); // The mesh has a radius of about 16 units
        belt.setSpeed( 0.002f );
        belt.setSeed( 1 );

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
            drawParticleSystemMenu( beltEntity.getComponent<Core::Scene::ParticleSystemComponent>(), flyCamera.getPose( cameraEntity.getComponent<Core::Scene::TransformComponent>() ) );

            // The particles are simulated with this delta time: without it they wouldn't move
            rasterizer->render( scene, cameraEntity, dt );
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
