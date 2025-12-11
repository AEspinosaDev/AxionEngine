#include "DX12Pipeline.hpp"
#include "DX12Debug.hpp"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN
namespace Graphics::RHI {

DX12PipelineLayout::DX12PipelineLayout( const ComPtr<ID3D12Device2>& device, const PipelineLayoutDesc& desc )
    : _desc( desc ) {
    buildRootSignature( device );
    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Pipeline Layout [{}] created", _desc.debugName );
}
DX12PipelineLayout::~DX12PipelineLayout() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Pipeline Layout [{}]", _desc.debugName );
}

uint DX12PipelineLayout::getViewCount( uint setIndex ) const {
    return setIndex < _desc.sets.size() ? _viewCountPerSet[setIndex] : 0;
}

uint DX12PipelineLayout::getSamplerCount( uint setIndex ) const {
    return setIndex < _desc.sets.size() ? _samplerCountPerSet[setIndex] : 0;
}

uint DX12PipelineLayout::getAccelCount( uint setIndex ) const {
    return setIndex < _desc.sets.size() ? _accelCountPerSet[setIndex] : 0;
}

void DX12PipelineLayout::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _rootSignature->SetName( std::wstring( ( _desc.debugName + " RootSig" ).begin(), ( _desc.debugName + " RootSig" ).end() ).c_str() );
}

NativeObject DX12PipelineLayout::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_RootSignature:
            return NativeObject( objectType, _rootSignature.Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Pipeline Layout | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12PipelineLayout::toString() const {
    return std::string();
}
void DX12PipelineLayout::buildRootSignature( const ComPtr<ID3D12Device2>& device ) {
    std::vector<CD3DX12_ROOT_PARAMETER1> rootParams;

    std::vector<CD3DX12_DESCRIPTOR_RANGE1> allRanges;
    allRanges.reserve( 64 );

    _rootIndexMap.resize( _desc.sets.size(), { -1, -1 } );
    _viewCountPerSet.resize( _desc.sets.size(), 0 );
    _samplerCountPerSet.resize( _desc.sets.size(), 0 );
    _accelCountPerSet.resize( _desc.sets.size(), 0 );

    for ( uint setIndex = 0; setIndex < _desc.sets.size(); ++setIndex )
    {
        const auto& set = _desc.sets[setIndex];

        std::vector<CD3DX12_DESCRIPTOR_RANGE1> viewRanges;
        std::vector<CD3DX12_DESCRIPTOR_RANGE1> samplerRanges;

        for ( const auto& binding : set.bindings )
        {
            if ( binding.type == DescriptorType::Sampler )
            {
                _samplerCountPerSet[setIndex] += binding.arraySize;
            } else
            {
                _viewCountPerSet[setIndex] += binding.arraySize;
            }

            D3D12_DESCRIPTOR_RANGE_TYPE rangeType = DX12Translator::get( binding.type );
            CD3DX12_DESCRIPTOR_RANGE1   range;

            range.Init(
                rangeType,
                binding.arraySize,
                binding.binding,
                setIndex,
                D3D12_DESCRIPTOR_RANGE_FLAG_NONE );

            if ( rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER )
                samplerRanges.push_back( range );
            else
                viewRanges.push_back( range );
        }

        if ( !viewRanges.empty() )
        {
            uint startIdx = (uint)allRanges.size();
            allRanges.insert( allRanges.end(), viewRanges.begin(), viewRanges.end() );

            CD3DX12_ROOT_PARAMETER1 param;
            param.InitAsDescriptorTable(
                (UINT)viewRanges.size(),
                &allRanges[startIdx],
                getShaderVisibility( set.bindings ) );
            rootParams.push_back( param );

            _rootIndexMap[setIndex].first = (int)rootParams.size() - 1;
        }

        if ( !samplerRanges.empty() )
        {
            uint startIdx = (uint)allRanges.size();
            allRanges.insert( allRanges.end(), samplerRanges.begin(), samplerRanges.end() );

            CD3DX12_ROOT_PARAMETER1 param;
            param.InitAsDescriptorTable(
                (UINT)samplerRanges.size(),
                &allRanges[startIdx],
                getShaderVisibility( set.bindings ) );
            rootParams.push_back( param );

            _rootIndexMap[setIndex].second = (int)rootParams.size() - 1;
        }
    }

    // Optional Push Constants
    if ( _desc.pushConstant.size > 0 )
    {
        CD3DX12_ROOT_PARAMETER1 pushParam;
        pushParam.InitAsConstants( _desc.pushConstant.size / 4, 0, 0, DX12Translator::get( _desc.pushConstant.stageMask ) );
        rootParams.push_back( pushParam );
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc {};
    rsDesc.Version                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rsDesc.Desc_1_1.NumParameters     = (UINT)rootParams.size();
    rsDesc.Desc_1_1.pParameters       = rootParams.data();
    rsDesc.Desc_1_1.NumStaticSamplers = 0;
    rsDesc.Desc_1_1.pStaticSamplers   = nullptr;
    rsDesc.Desc_1_1.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized, error;
    DX_CHECK( D3D12SerializeVersionedRootSignature( &rsDesc, &serialized, &error ) );
    DX_CHECK( device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS( &_rootSignature ) ) );
}

D3D12_SHADER_VISIBILITY DX12PipelineLayout::getShaderVisibility( const std::vector<DescriptorBinding>& bindings ) {
    ShaderStage mask = ShaderStage::None;
    for ( auto& b : bindings )
        mask |= b.stageMask;
    return DX12Translator::get( mask );
}

DX12GraphicPipeline::DX12GraphicPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc )
    : _desc( desc ) {

    // Basic validation: need at least vertex and pixel for graphics PSO
    const ShaderModule* vsModule = nullptr;
    const ShaderModule* psModule = nullptr;
    for ( const auto& m : desc.shaderModules )
    {
        if ( m.type == ShaderType::Vertex )
            vsModule = &m;
        if ( m.type == ShaderType::Pixel )
            psModule = &m;
    }

    AXION_LOG_ASSERT( vsModule && psModule, Logger::Module::RHI, "DX12 Graphic Pipeline requires at least VS and PS modules." );

    createPipelineState( device );

    setDebugName( _desc.debugName );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Graphic Pipeline [{}] created", _desc.debugName );
}
DX12GraphicPipeline::~DX12GraphicPipeline() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Graphic Pipeline [{}]", _desc.debugName );
}
void DX12GraphicPipeline::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _pso->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}
NativeObject DX12GraphicPipeline::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_PipelineState:
            return NativeObject( objectType, _pso.Get() );
        case ObjectTypes::DX12_RootSignature:
            return NativeObject( objectType, _desc.layout->getNativeObject( ObjectTypes::DX12_RootSignature ) );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Graphic Pipeline | Wrong Object Type" );
            return nullptr;
    }
}
std::string DX12GraphicPipeline::toString() const {
    return fmt::format( "" );
}
void DX12GraphicPipeline::createPipelineState( const ComPtr<ID3D12Device2>& device ) {

    const ShaderModule* vsModule = nullptr;
    const ShaderModule* psModule = nullptr;
    const ShaderModule* gsModule = nullptr;
    const ShaderModule* hsModule = nullptr;
    const ShaderModule* dsModule = nullptr;
    for ( const auto& m : _desc.shaderModules )
    {
        if ( m.type == ShaderType::Vertex )
            vsModule = &m;
        if ( m.type == ShaderType::Pixel )
            psModule = &m;
        if ( m.type == ShaderType::Geometry )
            gsModule = &m;
        if ( m.type == ShaderType::Hull )
            hsModule = &m;
        if ( m.type == ShaderType::Domain )
            dsModule = &m;
    }

    // Set up PSO desc
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory( &psoDesc, sizeof( psoDesc ) );
    AXION_LOG_ASSERT( _desc.layout, Logger::Module::RHI, "Fatal | No layout defined for Graphic Pipeline [{}]", _desc.debugName );
    psoDesc.pRootSignature = _desc.layout->getNativeObject( ObjectTypes::DX12_RootSignature );

    // Shader byte code blobs
    if ( vsModule )
    {
        psoDesc.VS.pShaderBytecode = vsModule->code;
        psoDesc.VS.BytecodeLength  = vsModule->codeSize;
    }
    if ( psModule )
    {
        psoDesc.PS.pShaderBytecode = psModule->code;
        psoDesc.PS.BytecodeLength  = psModule->codeSize;
    }
    if ( gsModule )
    {
        psoDesc.GS.pShaderBytecode = gsModule->code;
        psoDesc.GS.BytecodeLength  = gsModule->codeSize;
    }
    if ( hsModule )
    {
        psoDesc.HS.pShaderBytecode = hsModule->code;
        psoDesc.HS.BytecodeLength  = hsModule->codeSize;
    }
    if ( dsModule )
    {
        psoDesc.DS.pShaderBytecode = dsModule->code;
        psoDesc.DS.BytecodeLength  = dsModule->codeSize;
    }

    // Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> elems;
    D3D12_INPUT_LAYOUT_DESC               inputLayout = makeInputLayout( _desc, elems );
    psoDesc.InputLayout                               = inputLayout;

    // Primitive topology -> IAState
    psoDesc.PrimitiveTopologyType = DX12Translator::get( _desc.topology );

    // Rasterizer
    psoDesc.RasterizerState                       = {};
    psoDesc.RasterizerState.FillMode              = DX12Translator::get( _desc.rasterizerState.fillMode );
    psoDesc.RasterizerState.CullMode              = DX12Translator::get( _desc.rasterizerState.cullMode );
    psoDesc.RasterizerState.FrontCounterClockwise = _desc.rasterizerState.frontCounterClockwise;
    psoDesc.RasterizerState.DepthClipEnable       = _desc.rasterizerState.depthClipEnable;
    psoDesc.RasterizerState.MultisampleEnable     = _desc.rasterizerState.multisampleEnable;

    // Blend
    psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
    // override attachments if provided
    for ( size_t i = 0; i < _desc.blendState.attachments.size() && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i )
    {
        const auto& a             = _desc.blendState.attachments[i];
        auto&       dst           = psoDesc.BlendState.RenderTarget[i];
        dst.BlendEnable           = a.blendEnable;
        dst.RenderTargetWriteMask = a.writeMask;
        // map blend factors/op via translator helpers
        dst.SrcBlend       = DX12Translator::get( a.srcColor );
        dst.DestBlend      = DX12Translator::get( a.dstColor );
        dst.BlendOp        = DX12Translator::get( a.colorOp );
        dst.SrcBlendAlpha  = DX12Translator::get( a.srcAlpha );
        dst.DestBlendAlpha = DX12Translator::get( a.dstAlpha );
        dst.BlendOpAlpha   = DX12Translator::get( a.alphaOp );
    }

    // DepthStencil
    psoDesc.DepthStencilState.DepthEnable    = _desc.depthStencilState.depthEnable;
    psoDesc.DepthStencilState.DepthWriteMask = _desc.depthStencilState.depthWriteMask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc      = DX12Translator::get( _desc.depthStencilState.depthFunc );

    // Render target formats
    psoDesc.SampleMask       = _desc.sampleMask;
    psoDesc.SampleDesc.Count = _desc.sampleCount;

    for ( size_t i = 0; i < _desc.renderTargetFormats.size() && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i )
    {
        psoDesc.RTVFormats[i] = DX12Translator::get( _desc.renderTargetFormats[i] );
    }
    psoDesc.NumRenderTargets = (UINT)_desc.renderTargetFormats.size();
    psoDesc.DSVFormat        = DX12Translator::get( _desc.depthStencilFormat );

    // Create PSO
    DX_CHECK( device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &_pso ) ) );
}
D3D12_INPUT_LAYOUT_DESC DX12GraphicPipeline::makeInputLayout( const IGraphicPipeline::Description& desc, std::vector<D3D12_INPUT_ELEMENT_DESC>& out ) {
    out.clear();
    out.reserve( desc.attributes.size() );

    uint offset = 0;
    for ( const auto& a : desc.attributes )
    {
        D3D12_INPUT_ELEMENT_DESC e = {};
        e.SemanticName             = a.semanticName.c_str();
        e.SemanticIndex            = a.semanticIndex;
        e.Format                   = DX12Translator::get( a.format );
        e.InputSlot                = a.inputSlot;
        e.InputSlotClass           = a.instanceStepRate
                                         ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                         : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        e.InstanceDataStepRate     = a.instanceStepRate;
        // Handle offset
        // e.AlignedByteOffset = a.alignedByteOffset == AUTO_VAL ? D3D12_APPEND_ALIGNED_ELEMENT : a.alignedByteOffset;
        e.AlignedByteOffset = offset;
        offset += getFormatBytes( a.format );

        out.push_back( e );
    }

    D3D12_INPUT_LAYOUT_DESC ret;
    ret.pInputElementDescs = out.data();
    ret.NumElements        = static_cast<UINT>( out.size() );
    return ret;
}

DX12ComputePipeline::DX12ComputePipeline( const ComPtr<ID3D12Device2>& device, const Description& desc )
    : _desc( desc ) {
    // Basic validation: need at least vertex and pixel for graphics PSO
    const ShaderModule* compModule = nullptr;
    compModule                     = &_desc.shaderModule;

    AXION_LOG_ASSERT( compModule, Logger::Module::RHI, "DX12 Compute Pipeline requires a Compute module." );

    createPipelineState( device );

    setDebugName( _desc.debugName );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Compute Pipeline [{}] created", _desc.debugName );
}

DX12ComputePipeline::~DX12ComputePipeline() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Compute Pipeline [{}]", _desc.debugName );
}

void DX12ComputePipeline::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _pso->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

NativeObject DX12ComputePipeline::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_PipelineState:
            return NativeObject( objectType, _pso.Get() );
        case ObjectTypes::DX12_RootSignature:
            return NativeObject( objectType, _desc.layout->getNativeObject( ObjectTypes::DX12_RootSignature ) );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Compute Pipeline | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12ComputePipeline::toString() const {
    return std::string();
}

void DX12ComputePipeline::createPipelineState( const ComPtr<ID3D12Device2>& device ) {

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature                    = _desc.layout->getNativeObject( ObjectTypes::DX12_RootSignature );
    psoDesc.CS                                = { _desc.shaderModule.code, _desc.shaderModule.codeSize };
    psoDesc.Flags                             = D3D12_PIPELINE_STATE_FLAG_NONE;

    DX_CHECK( device->CreateComputePipelineState( &psoDesc, IID_PPV_ARGS( &_pso ) ) );
}

DX12RayTracingPipeline::DX12RayTracingPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc ) {

    // Basic validation: need at least vertex and pixel for graphics PSO
    const ShaderModule* rgenModule = nullptr;
    const ShaderModule* rmisModule = nullptr;
    const ShaderModule* chitModule = nullptr;
    for ( const auto& m : desc.shaderModules )
    {
        if ( m.type == ShaderType::RayGeneration )
            rgenModule = &m;
        if ( m.type == ShaderType::Miss )
            rmisModule = &m;
        if ( m.type == ShaderType::ClosestHit )
            chitModule = &m;
    }

    AXION_LOG_ASSERT( rgenModule && rmisModule, Logger::Module::RHI, "DX12 RT Pipeline requires at least RayGen and RayMiss shader modules." );
    if ( !chitModule )
        AXION_LOG_WARN( Logger::Module::RHI, "Creating DX12 RT Pipeline without closest hit shader ." );
    AXION_LOG_ASSERT( _desc.layout, Logger::Module::RHI, "DX12 RT Pipeline requires a Descriptor Layout." );

    ComPtr<ID3D12Device5> device5;
    device->QueryInterface( IID_PPV_ARGS( &device5 ) );

    createStateObject( device5 );

    setDebugName( _desc.debugName );

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 RayTracing Pipeline [{}] created", _desc.debugName );
}

DX12RayTracingPipeline::~DX12RayTracingPipeline() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 RayTracing Pipeline [{}]", _desc.debugName );
}

void* DX12RayTracingPipeline::getShaderIdentifier( const std::string& exportName ) const {
    if ( !_props )
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Attempting to get Shader ID from invalid pipeline props" );
        return nullptr;
    }

    std::wstring wName( exportName.begin(), exportName.end() );
    void*        id = _props->GetShaderIdentifier( wName.c_str() );

    if ( !id )
        AXION_LOG_ERROR( Logger::Module::RHI, "Shader Identifier [{}] not found in RT Pipeline [{}]", exportName, _desc.debugName );
    return id;
}

void DX12RayTracingPipeline::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    if ( _so )
        _so->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

NativeObject DX12RayTracingPipeline::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_StateObject:
        case ObjectTypes::DX12_PipelineState: // Legacy Fallback
            return NativeObject( objectType, _so.Get() );

        case ObjectTypes::DX12_RootSignature:
            return NativeObject( objectType, _desc.layout->getNativeObject( ObjectTypes::DX12_RootSignature ) );

        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 RayTracing Pipeline | Wrong Object Type" );
            return nullptr;
    }
}

std::string DX12RayTracingPipeline::toString() const {
    return std::string();
}

void DX12RayTracingPipeline::createStateObject( const ComPtr<ID3D12Device5>& device ) {

    CD3DX12_STATE_OBJECT_DESC dxrPipelineDesc( D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE );

    std::vector<std::wstring>          exportedNames;
    std::vector<D3D12_SHADER_BYTECODE> stableBytecodes;

    size_t moduleCount = _desc.shaderModules.size();
    exportedNames.reserve( moduleCount );
    stableBytecodes.reserve( moduleCount );

    // 2. DXIL Libraries Construction
    for ( const auto& mod : _desc.shaderModules )
    {
        // Filter for Ray Tracing shaders only
        bool isRTShader = ( mod.type == ShaderType::RayGeneration ||
                            mod.type == ShaderType::Miss ||
                            mod.type == ShaderType::ClosestHit ||
                            mod.type == ShaderType::AnyHit ||
                            mod.type == ShaderType::Intersection );

        if ( !isRTShader )
            continue;

        auto* lib = dxrPipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

        D3D12_SHADER_BYTECODE currentBytecode = {};
        currentBytecode.pShaderBytecode       = mod.code;
        currentBytecode.BytecodeLength        = mod.codeSize;

        stableBytecodes.push_back( currentBytecode );

        lib->SetDXILLibrary( &stableBytecodes.back() );

        // Handle Entry Point Name
        if ( !mod.entryPoint.empty() && mod.entryPoint != "main" )
        {
            exportedNames.emplace_back( mod.entryPoint.begin(), mod.entryPoint.end() );
            lib->DefineExport( exportedNames.back().c_str() );
        }
    }

    std::vector<std::wstring> hitGroupNames;
    std::vector<std::wstring> hitGroupImports;
    hitGroupNames.reserve( _desc.hitGroups.size() );
    hitGroupImports.reserve( _desc.hitGroups.size() * 3 );

    for ( const auto& hg : _desc.hitGroups )
    {
        auto* hitGroup = dxrPipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();

        // 1. Nombre del Hit Group (Export)
        hitGroupNames.emplace_back( hg.name.begin(), hg.name.end() );
        hitGroup->SetHitGroupExport( hitGroupNames.back().c_str() );

        hitGroup->SetHitGroupType( hg.isProcedural() ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE : D3D12_HIT_GROUP_TYPE_TRIANGLES );

        // 2. Closest Hit
        if ( !hg.closestHitShader.empty() )
        {
            hitGroupImports.emplace_back( hg.closestHitShader.begin(), hg.closestHitShader.end() );
            hitGroup->SetClosestHitShaderImport( hitGroupImports.back().c_str() );
        }

        // 3. Any Hit
        if ( !hg.anyHitShader.empty() )
        {
            hitGroupImports.emplace_back( hg.anyHitShader.begin(), hg.anyHitShader.end() );
            hitGroup->SetAnyHitShaderImport( hitGroupImports.back().c_str() );
        }

        // 4. Intersection
        if ( !hg.intersectionShader.empty() )
        {
            hitGroupImports.emplace_back( hg.intersectionShader.begin(), hg.intersectionShader.end() );
            hitGroup->SetIntersectionShaderImport( hitGroupImports.back().c_str() );
        }
    }

    auto* shaderConfig = dxrPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config( _desc.maxPayloadSize, _desc.maxAttributeSize );

    auto*                globalRootSigDesc = dxrPipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    ID3D12RootSignature* rootSig           = _desc.layout->getNativeObject( ObjectTypes::DX12_RootSignature );
    AXION_LOG_ASSERT( rootSig, Logger::Module::RHI, "Failed to retrieve RootSignature for RT Pipeline" );
    globalRootSigDesc->SetRootSignature( rootSig );

    auto* pipelineConfig = dxrPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineConfig->Config( _desc.maxDepth );

    DX_CHECK( device->CreateStateObject( dxrPipelineDesc, IID_PPV_ARGS( &_so ) ) );
    DX_CHECK( _so->QueryInterface( IID_PPV_ARGS( &_props ) ) );
}

} // namespace Graphics::RHI

AXION_NAMESPACE_END