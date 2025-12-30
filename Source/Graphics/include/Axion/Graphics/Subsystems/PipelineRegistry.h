#pragma once
#include "Axion/Graphics/RHI/Pipeline.h"
#include "Axion/Graphics/Subsystems/ShaderRegistry.h"
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Graphics {

/// @brief Central registry for creating, storing, and retrieving GPU Pipeline State Objects (PSOs).
/// Manages both Graphic and Compute pipelines, handling their lifecycle and hot-reloading.
class IPipelineRegistry
{
public:
    virtual ~IPipelineRegistry() = default;

    // Non-copyable / Non-movable (Owned by Renderer)
    IPipelineRegistry( const IPipelineRegistry& )            = delete;
    IPipelineRegistry& operator=( const IPipelineRegistry& ) = delete;
    IPipelineRegistry( IPipelineRegistry&& )                 = delete;
    IPipelineRegistry& operator=( IPipelineRegistry&& )      = delete;

    class GraphicBuilder;
    class ComputeBuilder;
    class RayTracingBuilder;

    // -------------------------------------------------------------------------
    // ENTRY POINTS
    // -------------------------------------------------------------------------

    /// @brief Starts the fluent construction of a Graphic Pipeline (Rasterization).
    /// @param name Unique debug name for the pipeline.
    virtual GraphicBuilder graphic( const std::string& name ) = 0;

    /// @brief Starts the fluent construction of a Compute Pipeline.
    /// @param name Unique debug name for the pipeline.
    virtual ComputeBuilder compute( const std::string& name ) = 0;

    /// @brief Starts the fluent construction of a Raytracing Pipeline.
    /// @param name Unique debug name for the pipeline.
    virtual RayTracingBuilder raytracing( const std::string& name ) = 0;

    // -------------------------------------------------------------------------
    // RUNTIME ACCESS
    // -------------------------------------------------------------------------

    /// @brief Retrieves the raw Graphic PSO pointer associated with a handle.
    /// @return Raw pointer or nullptr if handle is invalid or type mismatch.
    virtual RHI::IGraphicPipeline* getGraphicPipeline( PipelineHandle handle ) = 0;

    /// @brief Retrieves the raw Compute PSO pointer associated with a handle.
    /// @return Raw pointer or nullptr if handle is invalid or type mismatch.
    virtual RHI::IComputePipeline* getComputePipeline( PipelineHandle handle ) = 0;

    /// @brief Retrieves the raw Raytracing PSO pointer associated with a handle.
    /// @return Raw pointer or nullptr if handle is invalid or type mismatch.
    virtual RHI::IRayTracingPipeline* getRaytracingPipeline( PipelineHandle handle ) = 0;

    /// @brief Looks up a pipeline handle by its debug name.
    virtual std::optional<PipelineHandle> findPipeline( const std::string& name ) const = 0;

    /// @brief Destroys the pipeline resource and frees the slot.
    virtual void destroyPipeline( PipelineHandle handle ) = 0;

    /// @brief Returns the total number of registered pipelines.
    virtual uint size() const = 0;

    /// @brief Triggers hot-reloading for all pipelines.
    /// Recompiles linked shaders and recreates PSOs in-place.
    virtual void reloadAll() = 0;

protected:
    IPipelineRegistry() = default;

    // Internal factory methods called by Builders
    virtual PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, const std::string& shaderName )       = 0;
    virtual PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, const std::string& shaderName )       = 0;
    virtual PipelineHandle createRaytracing( RHI::RayTracingPipelineDesc& desc, const std::string& shaderName ) = 0;

    friend class GraphicBuilder;
    friend class ComputeBuilder;
    friend class RaytracingBuilder;
};

// -----------------------------------------------------------------------------
// GRAPHIC BUILDER
// -----------------------------------------------------------------------------

/// @brief Fluent builder for configuring Graphic Pipelines.
class IPipelineRegistry::GraphicBuilder
{
public:
    GraphicBuilder( IPipelineRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.debugName = std::move( name );
        // Default sane state
        _desc.rasterizerState   = { FillMode::Solid, CullMode::Back };
        _desc.depthStencilState = { true, true, CompareOp::Less };
        _desc.topology          = PrimitiveTopology::TriangleList;
    }

    /// @brief Sets the Shader Bundle to use (VS + PS).
    /// Looks up the shader in ShaderRegistry by name.
    GraphicBuilder& shader( const std::string& shaderName ) {
        _shaderName = shaderName;
        return *this;
    }

    /// @brief Appends a Render Target output description.
    GraphicBuilder& addRenderTarget( Format fmt, RHI::BlendAttachment blend = {} ) {
        _desc.renderTargetFormats.push_back( fmt );
        _desc.blendState.attachments.push_back( blend );
        return *this;
    }

    /// @brief Sets the Depth/Stencil buffer format.
    GraphicBuilder& setDepthFormat( Format fmt ) {
        _desc.depthStencilFormat = fmt;
        return *this;
    }

    // --- State Shortcuts ---

    /// @brief Sets FillMode to Wireframe.
    GraphicBuilder& wireframe() {
        _desc.rasterizerState.fillMode = FillMode::Wireframe;
        return *this;
    }

    /// @brief Disables face culling.
    GraphicBuilder& cullNone() {
        _desc.rasterizerState.cullMode = CullMode::None;
        return *this;
    }

    /// @brief Disables depth testing and writing.
    GraphicBuilder& disableDepth() {
        _desc.depthStencilState.depthEnable = false;
        return *this;
    }

    /// @brief Manually sets the full rasterizer state.
    GraphicBuilder& setRasterizer( const RHI::RasterizerState& state ) {
        _desc.rasterizerState = state;
        return *this;
    }

    /// @brief Sets the Root Signature / Pipeline Layout.
    /// @note If not called, the Registry will attempt to auto-generate it via Shader Reflection.
    GraphicBuilder& setLayout( RHI::IPipelineLayout* layout ) {
        _desc.layout = layout;
        return *this;
    }

    /// @brief Finalizes configuration and creates the PSO.
    PipelineHandle create() {
        return _registry.createGraphic( _desc, _shaderName );
    }

private:
    IPipelineRegistry&       _registry;
    RHI::GraphicPipelineDesc _desc;
    std::string              _shaderName;
};

// -----------------------------------------------------------------------------
// COMPUTE BUILDER
// -----------------------------------------------------------------------------

/// @brief Fluent builder for configuring Compute Pipelines.
class IPipelineRegistry::ComputeBuilder
{
public:
    ComputeBuilder( IPipelineRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.debugName = std::move( name );
    }

    /// @brief Sets the Compute Shader to use.
    /// Looks up the shader in ShaderRegistry by name.
    ComputeBuilder& shader( const std::string& shaderName ) {
        _shaderName = shaderName;
        return *this;
    }

    /// @brief Finalizes configuration and creates the PSO.
    PipelineHandle create() {
        return _registry.createCompute( _desc, _shaderName );
    }

private:
    IPipelineRegistry&       _registry;
    RHI::ComputePipelineDesc _desc;
    std::string              _shaderName;
};

// -----------------------------------------------------------------------------
// RAYTRACING BUILDER
// -----------------------------------------------------------------------------

class IPipelineRegistry::RayTracingBuilder
{
public:
    RayTracingBuilder( IPipelineRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.debugName      = std::move( name );
        _desc.maxDepth       = 8;
        _desc.maxPayloadSize = 256;
    }

    /// @brief Sets the Raytracing Shader to use.
    /// Looks up the shader in ShaderRegistry by name.
    RayTracingBuilder& shader( const std::string& shaderName ) {
        _shaderName = shaderName;
        return *this;
    }

    RayTracingBuilder& defineHitGroup(
        const std::string& groupName,
        const std::string& closestHitImport,
        const std::string& anyHitImport       = "",
        const std::string& intersectionImport = "" ) {
        _desc.hitGroups.push_back( { groupName, closestHitImport, anyHitImport, intersectionImport } );
        return *this;
    }

    RayTracingBuilder& setMaxDepth( uint depth ) {
        _desc.maxDepth = depth;
        return *this;
    }

    RayTracingBuilder& setPayloadSize( uint bytes ) {
        _desc.maxPayloadSize = bytes;
        return *this;
    }

    /// @brief Finalizes configuration and creates the PSO.
    PipelineHandle create() {
        return _registry.createRaytracing( _desc, _shaderName );
    }

private:
    IPipelineRegistry&          _registry;
    RHI::RayTracingPipelineDesc _desc;
    std::string                 _shaderName;
};

} // namespace Graphics
AXION_NAMESPACE_END