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
    class LayoutBuilder;

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

    /// @brief Starts the fluent construction of a Pipeline Layout.
    /// @param name Unique debug name for the layout.
    virtual LayoutBuilder layout( const std::string& name ) = 0;

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

    /// @brief Retrieves the raw layout pointer. Used internally by Builders.
    virtual RHI::IPipelineLayout* getLayout( PipelineLayoutHandle handle ) = 0;

    /// @brief Looks up a pipeline handle by its debug name.
    virtual std::optional<PipelineHandle> findPipeline( const std::string& name ) const = 0;

    /// @brief Destroys the pipeline resource and frees the slot.
    virtual void destroyPipeline( PipelineHandle handle ) = 0;

    /// @brief Returns the total number of registered pipelines.
    virtual uint size() const = 0;

    /// @brief Looks up a pipeline handle by its debug name.
    virtual std::optional<PipelineLayoutHandle> findLayout( const std::string& name ) const = 0;

    /// @brief Destroys the pipeline layout resource and frees the slot.
    virtual void destroyLayout( PipelineLayoutHandle handle ) = 0;

    /// @brief Triggers hot-reloading for all pipelines.
    /// Recompiles linked shaders and recreates PSOs in-place.
    virtual void reloadAll() = 0;

protected:
    IPipelineRegistry() = default;

    // Internal factory methods called by Builders
    virtual PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, ShaderHandle shaderHandle )           = 0;
    virtual PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, ShaderHandle shaderHandle )           = 0;
    virtual PipelineHandle createRaytracing( RHI::RayTracingPipelineDesc& desc, ShaderHandle shaderHandle )     = 0;
    virtual PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, const std::string& shaderName )       = 0;
    virtual PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, const std::string& shaderName )       = 0;
    virtual PipelineHandle createRaytracing( RHI::RayTracingPipelineDesc& desc, const std::string& shaderName ) = 0;

    virtual PipelineLayoutHandle createLayout( const RHI::PipelineLayoutDesc& desc ) = 0;

    friend class GraphicBuilder;
    friend class ComputeBuilder;
    friend class RaytracingBuilder;
    friend class LayoutBuilder;
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

    /// @brief Sets the Shader Bundle to use (VS + PS).
    /// Looks up the shader in ShaderRegistry by handle.
    GraphicBuilder& shader( ShaderHandle shaderHandle ) {
        _shaderHandle = shaderHandle;
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
    /// @brief Sets topology.
    GraphicBuilder& setTopology( PrimitiveTopology t ) {
        _desc.topology = t;
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
    GraphicBuilder& cullMode( CullMode mode ) {
        _desc.rasterizerState.cullMode = mode;
        return *this;
    }

    /// @brief Disables depth testing.
    GraphicBuilder& disableDepth() {
        _desc.depthStencilState.depthEnable = false;
        return *this;
    }

    /// @brief Manually sets the full rasterizer state.
    GraphicBuilder& setDepthStencilState( const RHI::DepthStencilState& depthState ) {
        _desc.depthStencilState = depthState;
        return *this;
    }

    /// @brief Manually sets the full rasterizer state.
    GraphicBuilder& setRasterizer( const RHI::RasterizerState& state ) {
        _desc.rasterizerState = state;
        return *this;
    }

    GraphicBuilder& setLayout( PipelineLayoutHandle handle ) {
        _desc.layout = _registry.getLayout( handle );
        return *this;
    }

    /// @brief Finalizes configuration and creates the PSO.
    PipelineHandle create() {
        return !_shaderName.empty() ? _registry.createGraphic( _desc, _shaderName ) : _registry.createGraphic( _desc, _shaderHandle );
    }

private:
    IPipelineRegistry&       _registry;
    RHI::GraphicPipelineDesc _desc;
    std::string              _shaderName;
    ShaderHandle             _shaderHandle;
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
    /// @brief Sets the Compute Shader to use.
    /// Looks up the shader in ShaderRegistry by handle.
    ComputeBuilder& shader( ShaderHandle shaderHandle ) {
        _shaderHandle = shaderHandle;
        return *this;
    }

    ComputeBuilder& setLayout( PipelineLayoutHandle handle ) {
        _desc.layout = _registry.getLayout( handle );
        return *this;
    }

    /// @brief Finalizes configuration and creates the PSO.
    PipelineHandle create() {
        return !_shaderName.empty() ? _registry.createCompute( _desc, _shaderName ) : _registry.createCompute( _desc, _shaderHandle );
    }

private:
    IPipelineRegistry&       _registry;
    RHI::ComputePipelineDesc _desc;
    std::string              _shaderName;
    ShaderHandle             _shaderHandle;
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

    /// @brief Sets the Raytracing Shader to use.
    /// Looks up the shader in ShaderRegistry by handle.
    RayTracingBuilder& shader( ShaderHandle shaderHandle ) {
        _shaderHandle = shaderHandle;
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

    RayTracingBuilder& setLayout( PipelineLayoutHandle handle ) {
        _desc.layout = _registry.getLayout( handle );
        return *this;
    }

    /// @brief Finalizes configuration and creates the PSO.
    PipelineHandle create() {
        return !_shaderName.empty() ? _registry.createRaytracing( _desc, _shaderName ) : _registry.createRaytracing( _desc, _shaderHandle );
    }

private:
    IPipelineRegistry&          _registry;
    RHI::RayTracingPipelineDesc _desc;
    std::string                 _shaderName;
    ShaderHandle                _shaderHandle;
};

// -----------------------------------------------------------------------------
// LAYOUT BUILDER
// -----------------------------------------------------------------------------

class IPipelineRegistry::LayoutBuilder
{
public:
    LayoutBuilder( IPipelineRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.debugName = std::move( name );
    }

    /// @brief Defines a descriptor set (space) with a list of bindings.
    LayoutBuilder& addSet( std::vector<RHI::DescriptorBinding> bindings ) {
        RHI::DescriptorLayoutDesc set;
        set.bindings = std::move( bindings );
        _desc.sets.push_back( std::move( set ) );
        return *this;
    }

    /// @brief Configures push constants / root constants.
    LayoutBuilder& setPushConstants( uint sizeBytes, uint registerIdx = 0, uint space = 0, RHI::ShaderStage mask = RHI::ShaderStage::All ) {
        _desc.pushConstant.size           = sizeBytes;
        _desc.pushConstant.customRegister = registerIdx;
        _desc.pushConstant.customSpace    = space;
        _desc.pushConstant.stageMask      = mask;
        return *this;
    }

    LayoutBuilder& enableIndirectRendering() {
        _desc.enableIndirectRendering = true;
        return *this;
    }

    /// @brief Finalizes configuration and creates the Layout.
    PipelineLayoutHandle create() {
        return _registry.createLayout( _desc );
    }

private:
    IPipelineRegistry&      _registry;
    RHI::PipelineLayoutDesc _desc;
};

} // namespace Graphics
AXION_NAMESPACE_END