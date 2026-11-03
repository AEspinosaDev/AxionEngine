#pragma once
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>

#include <Axion/Graphics/Platforms/IWindow.h>

#include <Axion/Graphics/RHI/IDevice.h>
#include <Axion/Graphics/RHI/IGUIBackend.h>

#include <Axion/Graphics/Subsystems/IGPUResourcePool.h>
#include <Axion/Graphics/Subsystems/IPipelineRegistry.h>
#include <Axion/Graphics/Subsystems/IRenderGraph.h>
#include <Axion/Graphics/Subsystems/IShaderRegistry.h>

AXION_NAMESPACE_BEGIN

namespace Graphics {

DEFINE_OWNER_PTR_FOR_TYPE( IRenderer, Renderer )

/// @brief High-level interface for the rendering engine.
/// Orquestrates the frame lifecycle, manages subsystems, and handles the RenderGraph execution.
class IRenderer
{
public:
    /// @brief Configuration settings for initializing the renderer.
    struct Settings {
        API           gfxApi           = API::DirectX12;        ///< Underlying Graphics API backend.
        BufferingType bufferingType    = BufferingType::Double; ///< Swapchain buffering (Double/Triple).
        bool          debugMode        = true;                  ///< Enable API validation layers and debug markers.
        PresentMode   presentMode      = PresentMode::Vsync;    ///< Presentation mode (Vsync/Immediate/Mailbox).
        Format        backbufferFormat = Format::RGBA8_UNORM;   ///< Swapchain backbuffer format.

        // General Memory Budgeting
        RendererMemoryBudget     memory           = {}; ///< Custom memory budget configuration.
        RendererDescriptorBudget descriptorBudget = {}; ///< Custom descriptor budget configuration.

        // SBT, Scracth & Uploa & RDG PER FRAME budgets (Must be below memory budget general limits)
        u64 maxSBTAlloc     = KBYTES( 1024 ); ///< Initial memory reservation for per-frame Shader Binding Tables data (1MB default).
        u64 maxStagingAlloc = MBYTES( 64 );   ///< Initial memory reservation for per-frame transient upload sensible data (Useful for CPU-GPU data streaming) (64MB default).
        u64 maxScratchAlloc = MBYTES( 64 );   ///< Initial memory reservation for per-frame transient upload sensible data (Useful for CPU-GPU data streaming) (64MB default).
        u64 RDGmaxAlloc     = KBYTES( 1024 ); ///< Initial memory reservation for per-frame RenderGraph data (1MB default).

        GCMode GCMode   = GCMode::AvgMemory; ///< Garbage Collection aggressiveness for transient resources.
        bool   autoSync = true;              ///< Automatic Barrier Insertion by RenderGraph.

        u32 selectedDeviceID = UINT32_MAX; ///<  Index of the GPU adapter to use. Set to UINT32_MAX for auto-selection (best dedicated GPU).

        bool enableGui = true; ///< Enable ImGui integration (Requires additional setup in the render loop).
    };

    virtual ~IRenderer() = default;

    // -------------------------------------------------------------------------
    // FRAME EXECUTION
    // -------------------------------------------------------------------------

    /// @brief Executes a single frame using the provided RenderGraph setup.
    /// Handles synchronization, swapchain acquisition, graph compilation, command recording, and presentation.
    /// @param setup Lambda function where the user defines passes, resoruces and descriptor sets using the builder.
    virtual void render( RenderGraphSetupFunc setup ) = 0;

    // -------------------------------------------------------------------------
    // SUBSYSTEM ACCESS
    // -------------------------------------------------------------------------

    /// @brief Access to the persistent GPU Resource Pool (Buffers/Textures/Samplers/AS).
    virtual IGPUResourcePool& resources() = 0;

    /// @brief Access to the Shader Registry (Compilation & Reflection).
    virtual IShaderRegistry& shaders() = 0;

    /// @brief Access to the Pipeline Registry (PSO Creation & Caching).
    virtual IPipelineRegistry& pipelines() = 0;

    // -------------------------------------------------------------------------
    // STATE & GETTERS
    // -------------------------------------------------------------------------

    /// @brief Returns the window associated with this renderer.
    virtual IWindow* getWindow() = 0;

    /// @brief Associates a new window with the renderer (triggers swapchain recreation).
    virtual void setWindow( IWindow* wnd ) = 0;

    /// @brief Returns the settings used to initialize the renderer.
    virtual const Settings& getSettings() const = 0;

    /// @brief Returns the texture handle of the current frame's swapchain image.
    /// Use this to import the backbuffer into the RenderGraph.
    virtual TextureHandle getCurrentBackbufferHandle() const = 0;

    /// @brief Returns current frame index.
    virtual u32 getCurrentFrameIndex() const = 0;

    /// @brief Returns frames in flight.
    virtual const u32 getTotalFramesInFlight() const = 0;

    /// @brief Returns the total number of frames rendered since initialization.
    virtual u64 getTotalFrameNumber() const = 0;

    /// @brief Returns the low-level RHI Device. Use only for advanced/raw access.
    virtual const RHI::DeviceOwnerPtr& getDevice() const = 0;

    /// @brief Returns the GUI Backend interface (e.g. ImGui). Returns nullptr if GUI integration is disabled or headless.
    virtual const RHI::IGUIBackend* getGUIBackend() const = 0;

    /// @brief Returns the renderer persistent allocator if persistent descriptor sets are needed (eg: general layout/bindless).
    /// Descriptors created with this allocator wont be reset after each frame
    virtual RHI::IDescriptorAllocator* const getDescriptorAllocator() = 0;

    // -------------------------------------------------------------------------
    // LIFECYCLE
    // -------------------------------------------------------------------------

    /// @brief Shuts down the renderer, releasing all GPU resources and contexts.
    virtual void destroy() = 0;

    /// @brief Returns a string representation of the renderer state/backend.
    virtual STLW::String toString() const = 0;

    /// @brief Returns true if the renderer was initialized without a window (Compute/Server mode).
    virtual bool isHeadless() = 0;

    // -------------------------------------------------------------------------
    // MISC
    // -------------------------------------------------------------------------

    /// @brief Executes a set of commands immediately on the GPU, bypassing the standard frame rendering loop.
    ///
    /// This method wraps the Device's internal `oneTimeSubmit`. It uses a dedicated, shared command list
    /// that is separate from the Renderer's per-frame command lists.
    ///
    /// @note This is a **synchronous (blocking)** operation. The function will not return until the GPU
    /// has finished executing these commands.
    ///
    /// @warning Use with caution during the main game loop. Since it forces a full CPU-GPU synchronization,
    /// calling this mid-frame will cause pipeline stalls. It is best suited for initialization,
    /// asset loading, or debugging tools.
    ///
    /// @param commands A lambda function containing the commands to record and execute.
    /// @return True if the execution completed successfully.
    virtual bool instantExecution( std::function<void( RHI::ICommandList* cmd )>& commands ) = 0;
};

using RendererSettings = IRenderer::Settings;

/// @brief Factory function to create a Renderer attached to a window.
RendererOwnerPtr createRenderer( IWindow* wnd, const RendererSettings& settings = {} );

/// @brief Factory function to create a Headless Renderer (no Swapchain).
RendererOwnerPtr createHeadlessRenderer( const RendererSettings& settings = {} );

} // namespace Graphics

AXION_NAMESPACE_END