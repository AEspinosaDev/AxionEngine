#pragma once
#include "Axion/Graphics/RHI/Pipeline.h"
#include "DX12Device.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( DX12PipelineLayout, DX12PipelineLayout )

class DX12PipelineLayout final : public RefCounter<IPipelineLayout>
{
public:
    DX12PipelineLayout( const ComPtr<ID3D12Device2>& device, const PipelineLayoutDesc& desc );
    ~DX12PipelineLayout() override;

    const Description& getDescription() const override { return _desc; }
    uint               getViewCount( uint setIndex ) const override;
    uint               getSamplerCount( uint setIndex ) const override;
    uint               getAccelCount( uint setIndex ) const override;
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

    std::pair<int, int> getRootIndices( uint setIndex ) const {
        if ( setIndex >= _rootIndexMap.size() )
            return { -1, -1 };
        return _rootIndexMap[setIndex];
    }

private:
    void                           buildRootSignature( const ComPtr<ID3D12Device2>& device );
    static D3D12_SHADER_VISIBILITY getShaderVisibility( const std::vector<DescriptorBinding>& bindings );

    PipelineLayoutDesc          _desc;
    ComPtr<ID3D12RootSignature> _rootSignature;

    std::vector<uint> _viewCountPerSet;
    std::vector<uint> _samplerCountPerSet;
    std::vector<uint> _accelCountPerSet;

    std::vector<std::pair<int, int>> _rootIndexMap;
};

DEFINE_COM_PTR_FOR_TYPE( DX12GraphicPipeline, DX12GraphicPipeline )

class DX12GraphicPipeline : public RefCounter<IGraphicPipeline>
{
public:
    DX12GraphicPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12GraphicPipeline() override;

    const Description& getDescription() const override { return _desc; }
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

private:
    void                           createPipelineState( const ComPtr<ID3D12Device2>& device );
    static D3D12_INPUT_LAYOUT_DESC makeInputLayout( const IGraphicPipeline::Description& desc, std::vector<D3D12_INPUT_ELEMENT_DESC>& out );

    Description                 _desc;
    ComPtr<ID3D12PipelineState> _pso;
};

DEFINE_COM_PTR_FOR_TYPE( DX12ComputePipeline, DX12ComputePipeline )

class DX12ComputePipeline : public RefCounter<IComputePipeline>
{
public:
    DX12ComputePipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12ComputePipeline() override;

    const Description& getDescription() const override { return _desc; }
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

private:
    void createPipelineState( const ComPtr<ID3D12Device2>& device );

    Description                 _desc;
    ComPtr<ID3D12PipelineState> _pso;
};

DEFINE_COM_PTR_FOR_TYPE( DX12RayTracingPipeline, DX12RayTracingPipeline )

class DX12RayTracingPipeline : public RefCounter<IRayTracingPipeline>
{
public:
    DX12RayTracingPipeline( const ComPtr<ID3D12Device2>& device, const Description& desc );
    ~DX12RayTracingPipeline() override;

    const Description& getDescription() const override { return _desc; }
    void*              getShaderIdentifier( const std::string& exportName ) const override;

    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override { return _desc.debugName; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    std::string        toString() const override;

private:
    void createStateObject( const ComPtr<ID3D12Device5>& device );

    Description _desc;

    ComPtr<ID3D12StateObject>           _so;
    ComPtr<ID3D12StateObjectProperties> _props;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END