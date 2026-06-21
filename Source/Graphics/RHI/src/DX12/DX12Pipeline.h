#pragma once
#include "Axion/Graphics/RHI/IPipeline.h"
#include "DX12Device.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_OWNER_PTR_FOR_TYPE( DX12PipelineLayout, DX12PipelineLayout )

class DX12PipelineLayout final : public IPipelineLayout
{
public:
    DX12PipelineLayout( const ComPtr<ID3D12Device2>& device, const PipelineLayoutDesc& desc );
    ~DX12PipelineLayout() override;

    const Description& getDescription() const override { return _desc; }
    u32                getViewCount( u32 setIndex ) const override;
    u32                getSamplerCount( u32 setIndex ) const override;
    u32                getAccelCount( u32 setIndex ) const override;
    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( StringView name ) override;
    StringView         getDebugName() const override;
    STLW::String       toString() const override;

    const D3D12BindingMappingLUT& getBindingMappingLUT( u32 setIndex ) const { return _bindingMappingPerSet[setIndex]; }

    std::pair<int, int> getRootIndices( u32 setIndex ) const {
        if ( setIndex >= _rootIndexMap.size() )
            return { -1, -1 };
        return _rootIndexMap[setIndex];
    }
    ID3D12CommandSignature* getIndirectCommandSignature() const { return _drawIndexedIndirectSignature.Get(); }
    ID3D12CommandSignature* getDispatchIndirectSignature() const { return _dispatchIndirectSignature.Get(); }

private:
    void                           buildRootSignature( const ComPtr<ID3D12Device2>& device );
    void                           buildIndirectCommandSignature( const ComPtr<ID3D12Device2>& device );
    static D3D12_SHADER_VISIBILITY getShaderVisibility( const SmallVector<DescriptorBinding>& bindings );

    PipelineLayoutDesc          _desc;
    ComPtr<ID3D12RootSignature> _rootSignature;

    // For Indirect Rendering
    int                            _pushConstantRootIndex = -1;
    ComPtr<ID3D12CommandSignature> _drawIndexedIndirectSignature;
    ComPtr<ID3D12CommandSignature> _dispatchIndirectSignature;

    SmallVector<u32, MAX_STACK_DESCRIPTOR_SET_COUNT> _viewCountPerSet;
    SmallVector<u32, MAX_STACK_DESCRIPTOR_SET_COUNT> _samplerCountPerSet;
    SmallVector<u32, MAX_STACK_DESCRIPTOR_SET_COUNT> _accelCountPerSet;

    SmallVector<std::pair<int, int>, MAX_STACK_DESCRIPTOR_SET_COUNT> _rootIndexMap;

    // Binging LUT for easy interop with descriptors
    SmallVector<D3D12BindingMappingLUT, MAX_STACK_DESCRIPTOR_SET_COUNT> _bindingMappingPerSet {};
};

DEFINE_OWNER_PTR_FOR_TYPE( DX12GraphicPipeline, DX12GraphicPipeline )

class DX12GraphicPipeline : public IGraphicPipeline
{
public:
    DX12GraphicPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12GraphicPipeline() override;

    const Description& getDescription() const override { return _desc; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( StringView name ) override;
    StringView         getDebugName() const override;
    STLW::String       toString() const override;

private:
    void                           createPipelineState( const ComPtr<ID3D12Device2>& device );
    static D3D12_INPUT_LAYOUT_DESC makeInputLayout( const IGraphicPipeline::Description& desc, STLW::Vector<D3D12_INPUT_ELEMENT_DESC>& out );

    Description                 _desc;
    ComPtr<ID3D12PipelineState> _pso;
};

DEFINE_OWNER_PTR_FOR_TYPE( DX12MeshPipeline, DX12MeshPipeline )

class DX12MeshPipeline final : public IMeshPipeline
{
public:
    DX12MeshPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12MeshPipeline() override;

    const MeshPipelineDesc& getDescription() const override { return _desc; }
    NativeObject            getNativeObject( ObjectType objectType ) override;
    void                    setDebugName( StringView name ) override;
    StringView              getDebugName() const override;
    STLW::String            toString() const override;

private:
    void createPipelineState( const ComPtr<ID3D12Device2>& device );

    struct alignas( void* ) MeshPipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_MS                    MS;
        CD3DX12_PIPELINE_STATE_STREAM_AS                    AS;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendState;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            RasterizerState;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencilState;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
    };

    Description                 _desc;
    ComPtr<ID3D12PipelineState> _pso;
};

DEFINE_OWNER_PTR_FOR_TYPE( DX12ComputePipeline, DX12ComputePipeline )

class DX12ComputePipeline : public IComputePipeline
{
public:
    DX12ComputePipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12ComputePipeline() override;

    const Description& getDescription() const override { return _desc; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( StringView name ) override;
    StringView         getDebugName() const override;
    STLW::String       toString() const override;

private:
    void createPipelineState( const ComPtr<ID3D12Device2>& device );

    Description                 _desc;
    ComPtr<ID3D12PipelineState> _pso;
};

DEFINE_OWNER_PTR_FOR_TYPE( DX12RayTracingPipeline, DX12RayTracingPipeline )

class DX12RayTracingPipeline : public IRayTracingPipeline
{
public:
    DX12RayTracingPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12RayTracingPipeline() override;

    const Description& getDescription() const override { return _desc; }
    void*              getShaderIdentifier( const StringView exportName ) const override;

    NativeObject getNativeObject( ObjectType objectType ) override;
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

private:
    void createStateObject( const ComPtr<ID3D12Device5>& device );

    Description _desc;

    ComPtr<ID3D12StateObject>           _so;
    ComPtr<ID3D12StateObjectProperties> _props;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END