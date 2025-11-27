#pragma once
#include "Axion/Graphics/RHI/Pipeline.h"
#include "Axion/Graphics/Subsystems/ShaderRegistry.h"
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Graphics {

class IPipelineRegistry
{
public:
    virtual ~IPipelineRegistry() = default;

    IPipelineRegistry( const IPipelineRegistry& )            = delete;
    IPipelineRegistry& operator=( const IPipelineRegistry& ) = delete;
    IPipelineRegistry( IPipelineRegistry&& )                 = delete;
    IPipelineRegistry& operator=( IPipelineRegistry&& )      = delete;

    class GraphicBuilder;
    class ComputeBuilder;

    virtual GraphicBuilder graphic( const std::string& name ) = 0;
    virtual ComputeBuilder compute( const std::string& name ) = 0;

    virtual RHI::IGraphicPipeline*        getGraphicPipeline( PipelineHandle handle )   = 0;
    virtual RHI::IComputePipeline*        getComputePipeline( PipelineHandle handle )   = 0;
    virtual std::optional<PipelineHandle> findPipeline( const std::string& name ) const = 0;
    virtual void                          destroyPipeline( PipelineHandle handle )      = 0;
    virtual uint                          size() const                                  = 0;

    virtual void reloadAll() = 0;

protected:
    IPipelineRegistry() = default;

    virtual PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, const std::string& shaderName ) = 0;
    virtual PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, const std::string& shaderName ) = 0;

    friend class GraphicBuilder;
    friend class ComputeBuilder;
};

class IPipelineRegistry::GraphicBuilder
{
public:
    GraphicBuilder( IPipelineRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.debugName         = std::move( name );
        _desc.rasterizerState   = { RHI::FillMode::Solid,
                                    RHI::CullMode::Back,
                                    /*...*/ };
        _desc.depthStencilState = { true, true, RHI::CompareOp::Less };
        _desc.topology          = RHI::PrimitiveTopology::TriangleList;
    }

    GraphicBuilder& shader( const std::string& shaderName ) {
        _shaderName = shaderName;
        return *this;
    }

    GraphicBuilder& addRenderTarget( Format fmt, RHI::BlendAttachment blend = {} ) {
        _desc.renderTargetFormats.push_back( fmt );
        _desc.blendState.attachments.push_back( blend );
        return *this;
    }
    GraphicBuilder& setDepthFormat( Format fmt ) {
        _desc.depthStencilFormat = fmt;
        return *this;
    }
    GraphicBuilder& wireframe() {
        _desc.rasterizerState.fillMode = RHI::FillMode::Wireframe;
        return *this;
    }
    GraphicBuilder& cullNone() {
        _desc.rasterizerState.cullMode = RHI::CullMode::None;
        return *this;
    }
    GraphicBuilder& disableDepth() {
        _desc.depthStencilState.depthEnable = false;
        return *this;
    }
    GraphicBuilder& setRasterizer( const RHI::RasterizerState& state ) {
        _desc.rasterizerState = state;
        return *this;
    }

    // Si no se llama, el Registry intentará crearlo via Reflexión de Slang
    GraphicBuilder& setLayout( RHI::IPipelineLayout* layout ) {
        _desc.layout = layout;
        return *this;
    }

    PipelineHandle create() {
        return _registry.createGraphic( _desc, _shaderName );
    }

private:
    IPipelineRegistry&       _registry;
    RHI::GraphicPipelineDesc _desc;

    std::string _shaderName;
};

class IPipelineRegistry::ComputeBuilder
{
public:
    ComputeBuilder( IPipelineRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.debugName = std::move( name );
    }

    /// @brief Define el shader de cómputo a utilizar (por nombre en ShaderRegistry).
    ComputeBuilder& shader( const std::string& shaderName ) {
        _shaderName = shaderName;
        return *this;
    }

    /// @brief Define manual del Layout. Si no se llama, se autogenera por reflexión.
    ComputeBuilder& setLayout( RHI::IPipelineLayout* layout ) {
        _desc.layout = layout;
        return *this;
    }

    /// @brief Construye el pipeline.
    PipelineHandle create() {
        return _registry.createCompute( _desc, _shaderName );
    }

private:
    IPipelineRegistry&       _registry;
    RHI::ComputePipelineDesc _desc;

    std::string _shaderName;
};

} // namespace Graphics
AXION_NAMESPACE_END