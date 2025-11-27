#include "PipelineRegistry.hpp"
#include "ShaderRegistry.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics {
PipelineRegistry::PipelineRegistry( RHI::IDevice* device, IShaderRegistry& shaderReg )
    : _device( device )
    , _shaderReg( shaderReg ) {
    AXION_LOG_INFO( Logger::Module::GFX, "Pipeline Registry Subsystem Initialized Succesfully" );
}

PipelineRegistry::~PipelineRegistry() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Pipeline Registry" );
}
PipelineHandle PipelineRegistry::createGraphic( RHI::GraphicPipelineDesc& desc, const std::string& shaderName ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToHandle.find( desc.debugName ); it != _nameToHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline [{}] already exists.", desc.debugName );
        return it->second;
    }

    RHI::PipelineLayoutPtr layoutPtr    = nullptr;
    auto                   shaderHandle = _shaderReg.findShader( shaderName );
    if ( !shaderHandle.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader [{}] not found for pipeline.", shaderName );
        return {};
    }
    const auto& shaderBundle = _shaderReg.getBundle( shaderHandle.value() );
    desc.shaderModules.clear();
    desc.shaderModules.reserve( shaderBundle.stageBlobs.size() );
    for ( const auto& [type, blob] : shaderBundle.stageBlobs )
    {
        desc.shaderModules.push_back( { .type       = type, // Casting de Stage a Type
                                        .code       = blob.code.data(),
                                        .codeSize   = blob.code.size(),
                                        .entryPoint = blob.entryPointName } );
    }

    if ( desc.attributes.empty() )
        desc.attributes = shaderBundle.vertexAttributes;

    layoutPtr   = _device->createPipelineLayout( shaderBundle.layoutDesc );
    desc.layout = layoutPtr.get();

    auto pipelinePtr = _device->createGraphicPipeline( desc );
    if ( !pipelinePtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed creation for Pipeline [{}]", desc.debugName );
        return {};
    }

    uint id = UINT32_MAX;
    for ( uint i = 0; i < _pipelines.size(); ++i )
    {
        if ( !_pipelines[i].alive )
        {
            id = i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_pipelines.size();
        _pipelines.emplace_back();
    }

    // Rellenar Record
    auto& record       = _pipelines[id];
    record.name        = desc.debugName;
    record.alive       = true;
    record.pipeline    = std::move( pipelinePtr );
    record.layoutOwner = std::move( layoutPtr );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Graphic Pipeline [{}]", desc.debugName );

    return PipelineHandle { id };
}

PipelineHandle PipelineRegistry::createCompute( RHI::ComputePipelineDesc& desc, const std::string& shaderName ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToHandle.find( desc.debugName ); it != _nameToHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline [{}] already exists.", desc.debugName );
        return it->second;
    }

    auto shaderHandle = _shaderReg.findShader( shaderName );
    if ( !shaderHandle.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader [{}] not found for pipeline.", shaderName );
        return {};
    }

    const auto& shaderBundle = _shaderReg.getBundle( shaderHandle.value() );

    auto itStage = shaderBundle.stageBlobs.find( RHI::ShaderType::Compute );

    if ( itStage == shaderBundle.stageBlobs.end() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader Bundle [{}] does not contain a Compute Stage!", shaderName );
        return {};
    }

    const auto& blob = itStage->second;

    desc.shaderModule = {
        .type       = RHI::ShaderType::Compute,
        .code       = blob.code.data(),
        .codeSize   = blob.code.size(),
        .entryPoint = blob.entryPointName };

    RHI::PipelineLayoutPtr layoutPtr = _device->createPipelineLayout( shaderBundle.layoutDesc );
    desc.layout                      = layoutPtr.get();

    auto pipelinePtr = _device->createComputePipeline( desc );
    if ( !pipelinePtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed creation for Compute Pipeline [{}]", desc.debugName );
        return {};
    }

    uint32_t id = UINT32_MAX;
    for ( uint32_t i = 0; i < _pipelines.size(); ++i )
    {
        if ( !_pipelines[i].alive )
        {
            id = i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint32_t)_pipelines.size();
        _pipelines.emplace_back();
    }

    auto& record       = _pipelines[id];
    record.name        = desc.debugName;
    record.alive       = true;
    record.pipeline    = std::move( pipelinePtr );
    record.layoutOwner = std::move( layoutPtr );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Compute Pipeline [{}]", desc.debugName );

    return PipelineHandle { id };
}

RHI::IGraphicPipeline* PipelineRegistry::getGraphicPipeline( PipelineHandle handle ) {
    if ( handle.id >= _pipelines.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid PipelineHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _pipelines[handle.id];

    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead PipelineHandle" );
        return nullptr;
    }

    auto* pipPtr = std::get_if<RHI::GraphicPipelinePtr>( &record.pipeline );

    if ( pipPtr )
    {
        return pipPtr->get();
    }

    return nullptr;
}

RHI::IComputePipeline* PipelineRegistry::getComputePipeline( PipelineHandle handle ) {
    if ( handle.id >= _pipelines.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid PipelineHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _pipelines[handle.id];

    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead PipelineHandle" );
        return nullptr;
    }

    auto* pipPtr = std::get_if<RHI::ComputePipelinePtr>( &record.pipeline );

    if ( pipPtr )
    {
        return pipPtr->get();
    }

    return nullptr;
}

std::optional<PipelineHandle> PipelineRegistry::findPipeline( const std::string& name ) const {
    auto it = _nameToHandle.find( name );
    if ( it == _nameToHandle.end() )
        return std::nullopt;
    return it->second;
}

void PipelineRegistry::destroyPipeline( PipelineHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _pipelines.size() )
        return;
    auto& record = _pipelines[handle.id];

    if ( !record.alive )
        return;

    _nameToHandle.erase( record.name );

    record.alive = false;
    record.name.clear();

    record.pipeline    = std::monostate {};
    record.layoutOwner = nullptr;

    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Pipeline Handle [{}]", handle.id );
}

void PipelineRegistry::reloadAll() {
}

} // namespace Graphics
AXION_NAMESPACE_END
