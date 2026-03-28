#include "PipelineRegistry.h"
#include "ShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {
PipelineRegistry::PipelineRegistry()
    : RendererSubsystem() {
}

PipelineRegistry::~PipelineRegistry() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Pipeline Registry" );
}

void PipelineRegistry::initialize( const SubsystemInitContext& ctx ) {
    RendererSubsystem::initialize( ctx );
    _shaderReg = ctx.shaderReg;
    AXION_LOG_INFO( Logger::Module::GFX, "Pipeline Registry Subsystem Initialized Succesfully" );
}

PipelineHandle PipelineRegistry::createGraphic( RHI::GraphicPipelineDesc& desc, ShaderHandle shaderHandle ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToHandle.find( desc.debugName ); it != _nameToHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline [{}] already exists.", desc.debugName );
        return it->second;
    }

    const auto& shaderBundle = _shaderReg->getBundle( shaderHandle );

    desc.shaderModules.clear();
    desc.shaderModules.reserve( shaderBundle.stageBlobs.size() );
    for ( const auto& blob : shaderBundle.stageBlobs )
    {
        desc.shaderModules.push_back( { .type       = blob.type, // Casting de Stage a Type
                                        .code       = blob.code.data(),
                                        .codeSize   = blob.code.size(),
                                        .entryPoint = blob.entryPointName } );
    }

    if ( desc.attributes.empty() )
        desc.attributes = shaderBundle.vertexAttributes;

    RHI::PipelineLayoutOwnerPtr implicitLayoutOwner = nullptr;
    if ( desc.layout != nullptr )
    {
        // Explicit
    } else
    {
        implicitLayoutOwner = _device->createPipelineLayout( shaderBundle.layoutDesc );
        desc.layout         = implicitLayoutOwner.get();
    }

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
    record.layoutOwner = std::move( implicitLayoutOwner );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Graphic Pipeline [{}]", desc.debugName );

    return PipelineHandle { id };
}

PipelineHandle PipelineRegistry::createGraphic( RHI::GraphicPipelineDesc& desc, const std::string& shaderName ) {
    auto shaderHandleOpt = _shaderReg->findShader( shaderName );

    if ( !shaderHandleOpt.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader [{}] not found when creating Graphic pipeline [{}].", shaderName, desc.debugName );
        return {};
    }

    return createGraphic( desc, shaderHandleOpt.value() );
}

PipelineHandle PipelineRegistry::createCompute( RHI::ComputePipelineDesc& desc, ShaderHandle shaderHandle ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToHandle.find( desc.debugName ); it != _nameToHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline [{}] already exists.", desc.debugName );
        return it->second;
    }

    const auto& shaderBundle = _shaderReg->getBundle( shaderHandle );

    auto itStage = std::find_if(
        shaderBundle.stageBlobs.begin(),
        shaderBundle.stageBlobs.end(),
        []( const auto& blob ) {
            return blob.type == ShaderType::Compute;
        } );

    if ( itStage == shaderBundle.stageBlobs.end() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader Bundle [{}] does not contain a Compute Stage!", desc.debugName );
        return {};
    }

    // Desreferenciamos el iterador para obtener el StageBlob&
    const auto& blob = *itStage;

    desc.shaderModule = {
        .type       = ShaderType::Compute,
        .code       = blob.code.data(),
        .codeSize   = blob.code.size(),
        .entryPoint = blob.entryPointName };

    RHI::PipelineLayoutOwnerPtr implicitLayoutOwner = nullptr;
    if ( desc.layout != nullptr )
    {
        // Explicit
    } else
    {
        implicitLayoutOwner = _device->createPipelineLayout( shaderBundle.layoutDesc );
        desc.layout         = implicitLayoutOwner.get();
    }

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
    record.layoutOwner = std::move( implicitLayoutOwner );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Compute Pipeline [{}]", desc.debugName );

    return PipelineHandle { id };
}

PipelineHandle PipelineRegistry::createCompute( RHI::ComputePipelineDesc& desc, const std::string& shaderName ) {
    auto shaderHandleOpt = _shaderReg->findShader( shaderName );

    if ( !shaderHandleOpt.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader [{}] not found when creating Compute pipeline [{}].", shaderName, desc.debugName );
        return {};
    }

    return createCompute( desc, shaderHandleOpt.value() );
}

PipelineHandle PipelineRegistry::createRaytracing( RHI::RayTracingPipelineDesc& desc, ShaderHandle shaderHandle ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToHandle.find( desc.debugName ); it != _nameToHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline [{}] already exists.", desc.debugName );
        return it->second;
    }

    const auto& shaderBundle = _shaderReg->getBundle( shaderHandle );

    desc.shaderModules.clear();
    desc.shaderModules.reserve( shaderBundle.stageBlobs.size() );
    for ( const auto& blob : shaderBundle.stageBlobs )
    {
        desc.shaderModules.push_back( { .type       = blob.type, // Casting de Stage a Type
                                        .code       = blob.code.data(),
                                        .codeSize   = blob.code.size(),
                                        .entryPoint = blob.entryPointName } );
    }

    RHI::PipelineLayoutOwnerPtr implicitLayoutOwner = nullptr;
    if ( desc.layout != nullptr )
    {
        // Explicit
    } else
    {
        implicitLayoutOwner = _device->createPipelineLayout( shaderBundle.layoutDesc );
        desc.layout         = implicitLayoutOwner.get();
    }

    auto pipelinePtr = _device->createRayTracingPipeline( desc );
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
    record.layoutOwner = std::move( implicitLayoutOwner );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Raytracing Pipeline [{}]", desc.debugName );

    return PipelineHandle { id };
}
PipelineHandle PipelineRegistry::createMesh( RHI::MeshPipelineDesc& desc, ShaderHandle shaderHandle ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToHandle.find( desc.debugName ); it != _nameToHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline [{}] already exists.", desc.debugName );
        return it->second;
    }

    const auto& shaderBundle = _shaderReg->getBundle( shaderHandle );

    desc.shaderModules.clear();
    desc.shaderModules.reserve( shaderBundle.stageBlobs.size() );
    for ( const auto& blob : shaderBundle.stageBlobs )
    {
        desc.shaderModules.push_back( { .type       = blob.type, // Casting de Stage a Type
                                        .code       = blob.code.data(),
                                        .codeSize   = blob.code.size(),
                                        .entryPoint = blob.entryPointName } );
    }

    RHI::PipelineLayoutOwnerPtr implicitLayoutOwner = nullptr;
    if ( desc.layout != nullptr )
    {
        // Explicit
    } else
    {
        implicitLayoutOwner = _device->createPipelineLayout( shaderBundle.layoutDesc );
        desc.layout         = implicitLayoutOwner.get();
    }

    auto pipelinePtr = _device->createMeshPipeline( desc );
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
    record.layoutOwner = std::move( implicitLayoutOwner );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Mesh Pipeline [{}]", desc.debugName );

    return PipelineHandle { id };
}
PipelineHandle PipelineRegistry::createRaytracing( RHI::RayTracingPipelineDesc& desc, const std::string& shaderName ) {
    auto shaderHandleOpt = _shaderReg->findShader( shaderName );

    if ( !shaderHandleOpt.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader [{}] not found when creating Raytracing pipeline [{}].", shaderName, desc.debugName );
        return {};
    }

    return createRaytracing( desc, shaderHandleOpt.value() );
}

PipelineHandle PipelineRegistry::createMesh( RHI::MeshPipelineDesc& desc, const std::string& shaderName ) {
    auto shaderHandleOpt = _shaderReg->findShader( shaderName );

    if ( !shaderHandleOpt.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Shader [{}] not found when creating Mesh pipeline [{}].", shaderName, desc.debugName );
        return {};
    }

    return createMesh( desc, shaderHandleOpt.value() );
}

PipelineLayoutHandle PipelineRegistry::createLayout( const RHI::PipelineLayoutDesc& desc ) {
    std::scoped_lock lock( _mutex );

    if ( auto it = _nameToLayoutHandle.find( desc.debugName ); it != _nameToLayoutHandle.end() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Pipeline Layout [{}] already exists. Returning existing handle.", desc.debugName );
        return it->second;
    }

    auto layoutPtr = _device->createPipelineLayout( desc );
    if ( !layoutPtr )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to create Pipeline Layout [{}]", desc.debugName );
        return {};
    }

    // 3. Register
    uint id = UINT32_MAX;
    for ( uint i = 0; i < _layouts.size(); ++i )
    {
        if ( !_layouts[i].alive )
        {
            id = i;
            break;
        }
    }

    if ( id == UINT32_MAX )
    {
        id = (uint)_layouts.size();
        _layouts.emplace_back();
    }

    // Rellenar Record
    auto& record  = _layouts[id];
    record.name   = desc.debugName;
    record.alive  = true;
    record.layout = std::move( layoutPtr );

    _nameToHandle[desc.debugName] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Pipeline Layout [{}]", desc.debugName );

    return { id };
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

    auto* pipPtr = std::get_if<RHI::GraphicPipelineOwnerPtr>( &record.pipeline );

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

    auto* pipPtr = std::get_if<RHI::ComputePipelineOwnerPtr>( &record.pipeline );

    if ( pipPtr )
    {
        return pipPtr->get();
    }

    return nullptr;
}

RHI::IRayTracingPipeline* PipelineRegistry::getRaytracingPipeline( PipelineHandle handle ) {
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

    auto* pipPtr = std::get_if<RHI::RayTracingPipelineOwnerPtr>( &record.pipeline );

    if ( pipPtr )
    {
        return pipPtr->get();
    }

    return nullptr;
}

RHI::IMeshPipeline* PipelineRegistry::getMeshPipeline( PipelineHandle handle ) {
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

    auto* pipPtr = std::get_if<RHI::MeshPipelineOwnerPtr>( &record.pipeline );

    if ( pipPtr )
    {
        return pipPtr->get();
    }

    return nullptr;
}

RHI::IPipelineLayout* PipelineRegistry::getLayout( PipelineLayoutHandle handle ) {
    if ( handle.id >= _layouts.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid PipelineLayoutHandle ID: {}", handle.id );
        return nullptr;
    }

    auto& record = _layouts[handle.id];

    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead PipelineLayoutHandle" );
        return nullptr;
    }

    if ( record.layout )
    {
        return record.layout.get();
    }

    return nullptr;
}

std::optional<PipelineHandle> PipelineRegistry::findPipeline( const std::string& name ) const {
    auto it = _nameToHandle.find( name );
    if ( it == _nameToHandle.end() )
        return std::nullopt;
    return it->second;
}

std::optional<PipelineLayoutHandle> PipelineRegistry::findLayout( const std::string& name ) const {
    auto it = _nameToLayoutHandle.find( name );
    if ( it == _nameToLayoutHandle.end() )
        return std::nullopt;
    return it->second;
}

void PipelineRegistry::destroyLayout( PipelineLayoutHandle handle ) {
    std::scoped_lock lock( _mutex );

    if ( handle.id >= _layouts.size() )
        return;
    auto& record = _layouts[handle.id];

    if ( !record.alive )
        return;

    _nameToLayoutHandle.erase( record.name );

    record.alive = false;
    record.name.clear();

    record.layout = nullptr;

    AXION_LOG_INFO( Logger::Module::GFX, "Destroyed Pipeline Layout Handle [{}]", handle.id );
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
