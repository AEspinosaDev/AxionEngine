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
    std::vector<CD3DX12_ROOT_PARAMETER1>   rootParams;
    std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;

    // Reserve space
    rootParams.reserve( _desc.sets.size() );
    ranges.reserve( 32 );

    for ( uint32_t setIndex = 0; setIndex < _desc.sets.size(); ++setIndex )
    {
        const auto& set = _desc.sets[setIndex];

        // Create one descriptor table per set
        std::vector<CD3DX12_DESCRIPTOR_RANGE1> setRanges;
        for ( const auto& binding : set.bindings )
        {
            D3D12_DESCRIPTOR_RANGE_TYPE rangeType = DX12Translator::get( binding.type );

            CD3DX12_DESCRIPTOR_RANGE1 range;
            range.Init(
                rangeType,
                binding.arraySize,
                binding.binding, // register index
                setIndex,        // space = descriptor set index
                D3D12_DESCRIPTOR_RANGE_FLAG_NONE );
            setRanges.push_back( range );
        }

        uint32_t baseRangeIndex = static_cast<uint32_t>( ranges.size() );
        ranges.insert( ranges.end(), setRanges.begin(), setRanges.end() );

        CD3DX12_ROOT_PARAMETER1 param;
        param.InitAsDescriptorTable(
            static_cast<UINT>( setRanges.size() ),
            &ranges[baseRangeIndex],
            getShaderVisibility( set.bindings ) );
        rootParams.push_back( param );
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

    AXION_LOG_ASSERT( vsModule && psModule, Logger::Module::GFX, "DX12 Graphic Pipeline requires at least VS and PS modules." );

    // createRootSignature( device );
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
        e.AlignedByteOffset = a.alignedByteOffset == AUTO_VAL ? D3D12_APPEND_ALIGNED_ELEMENT : a.alignedByteOffset;

        out.push_back( e );

        offset += getFormatBytes( a.format );
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

    AXION_LOG_ASSERT( compModule, Logger::Module::GFX, "DX12 Compute Pipeline requires a Compute module." );

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

} // namespace Graphics::RHI

AXION_NAMESPACE_END