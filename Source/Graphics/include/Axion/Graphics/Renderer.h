#pragma once
#include "Axion/Graphics/Platforms/Window.h"
#include "Axion/Graphics/RHI/Device.h"
#include "Axion/Graphics/Subsystems/GPUResourcePool.h"
#include "Axion/Graphics/Subsystems/PipelineRegistry.h"
#include "Axion/Graphics/Subsystems/RenderGraph.h"
#include "Axion/Graphics/Subsystems/ShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

DEFINE_SHARED_PTR_FOR_TYPE( IRenderer, Renderer )

/// @brief High-level interface for the rendering engine.
/// Orquestrates the frame lifecycle, manages subsystems, and handles the RenderGraph execution.
class IRenderer
{
public:
    /// @brief Configuration settings for initializing the renderer.
    struct Settings {
        API           gfxApi                = API::DirectX12;        ///< Underlying Graphics API backend.
        BufferingType bufferingType         = BufferingType::Double; ///< Swapchain buffering (Double/Triple).
        bool          debugMode             = true;                  ///< Enable API validation layers and debug markers.
        PresentMode   presentMode           = PresentMode::Vsync;    ///< Presentation mode (Vsync/Immediate/Mailbox).
        Format        backbufferFormat      = Format::RGBA8_UNORM;   ///< Swapchain backbuffer format.
        ulong         RGAllocSize           = 1024 * 1024;           ///< Initial memory reservation for per-frame RenderGraph data (1MB default).
        ulong         RGAllocSBTSize        = 1024 * 1024;           ///< Initial memory reservation for per-frame Shader Binding Tables data (1MB default).
        uint          RGDescriptorsPerFrame = 2048;                  ///< Initial memory reservation for per-frame DescriptorSet data.
        GCMode        GCMode                = GCMode::AvgMemory;     ///< Garbage Collection aggressiveness for transient resources.
        bool          autoSync              = true;                  ///< Automatic Barrier Insertion.
    };

    virtual ~IRenderer() = default;

    // -------------------------------------------------------------------------
    // FRAME EXECUTION
    // -------------------------------------------------------------------------

    /// @brief Executes a single frame using the provided RenderGraph setup.
    /// Handles synchronization, swapchain acquisition, graph compilation, command recording, and presentation.
    /// @param setup Lambda function where the user defines passes and resources using the builder.
    virtual void render( RenderGraphSetupFunc setup ) = 0;

    // -------------------------------------------------------------------------
    // SUBSYSTEM ACCESS
    // -------------------------------------------------------------------------

    /// @brief Access to the persistent GPU Resource Pool (Buffers/Textures).
    virtual IGPUResourcePool& resources() = 0;

    /// @brief Access to the Shader Registry (Compilation & Reflection).
    virtual IShaderRegistry& shaders() = 0;

    /// @brief Access to the Pipeline Registry (PSO Creation & Caching).
    virtual IPipelineRegistry& pipelines() = 0;

    // -------------------------------------------------------------------------
    // STATE & GETTERS
    // -------------------------------------------------------------------------

    /// @brief Returns the window associated with this renderer.
    virtual const WindowPtr& getWindow() = 0;

    /// @brief Associates a new window with the renderer (triggers swapchain recreation).
    virtual void setWindow( const WindowPtr& wnd ) = 0;

    /// @brief Returns the settings used to initialize the renderer.
    virtual const Settings& getSettings() const = 0;

    /// @brief Returns the low-level RHI Device. Use only for advanced/raw access.
    virtual const RHI::DevicePtr& getDevice() const = 0;

    /// @brief Returns the texture handle of the current frame's swapchain image.
    /// Use this to import the backbuffer into the RenderGraph.
    virtual TextureHandle getCurrentBackbufferHandle() const = 0;

    /// @brief Returns current frame index.
    virtual ulong getCurrentFrameIndex() const = 0;

    /// @brief Returns the total number of frames rendered since initialization.
    virtual ulong getTotalFrameNumber() const = 0;

    // -------------------------------------------------------------------------
    // LIFECYCLE
    // -------------------------------------------------------------------------

    /// @brief Shuts down the renderer, releasing all GPU resources and contexts.
    virtual void destroy() = 0;

    /// @brief Returns a string representation of the renderer state/backend.
    virtual std::string toString() const = 0;

    /// @brief Returns true if the renderer was initialized without a window (Compute/Server mode).
    virtual bool isHeadless() = 0;
};

using RendererSettings = IRenderer::Settings;

/// @brief Factory function to create a Renderer attached to a window.
RendererPtr createRenderer( const WindowPtr& wnd, const RendererSettings& settings = {} );

/// @brief Factory function to create a Headless Renderer (no Swapchain).
RendererPtr createHeadlessRenderer( const RendererSettings& settings = {} );

} // namespace Graphics

AXION_NAMESPACE_END