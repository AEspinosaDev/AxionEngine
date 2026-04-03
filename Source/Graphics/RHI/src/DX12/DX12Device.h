#pragma once
#include "DX12Common.h"
#include "DX12Descriptor.h"
#include <Axion/Graphics/RHI/DX12/IDX12Device.h>
#include <functional>

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12Device final : public IDX12Device
{

public:
    DX12Device( const IDX12Device::Description& desc );
    ~DX12Device() override;

    SwapchainOwnerPtr          createSwapchain( const NativeObject& Ptr, const SwapchainDesc& desc = {} ) override;
    CommandListOwnerPtr        createCommandList( const CommandListDesc& desc ) override;
    TextureOwnerPtr            createTexture( const TextureDesc& desc, const void* initialData = nullptr ) override;
    BufferOwnerPtr             createBuffer( const BufferDesc& desc, const void* initialData = nullptr ) override;
    SamplerOwnerPtr            createSampler( const SamplerDesc& desc ) override;
    AccelOwnerPtr              createAccel( const AccelDesc& desc, bool immediateBuild = false ) override;
    PipelineLayoutOwnerPtr     createPipelineLayout( const PipelineLayoutDesc& desc ) override;
    GraphicPipelineOwnerPtr    createGraphicPipeline( const GraphicPipelineDesc& desc ) override;
    ComputePipelineOwnerPtr    createComputePipeline( const ComputePipelineDesc& desc ) override;
    RayTracingPipelineOwnerPtr createRayTracingPipeline( const RayTracingPipelineDesc& desc ) override;
    MeshPipelineOwnerPtr       createMeshPipeline( const MeshPipelineDesc& desc ) override;

    DescriptorAllocatorOwnerPtr createDescriptorAllocator( const DescriptorAllocatorDesc& desc ) override;
    SBTAllocatorOwnerPtr        createSBTAllocator( const SBTAllocatorDesc& desc ) override;
    TransientAllocatorOwnerPtr  createTransientAllocator( const TransientAllocatorDesc& desc ) override;

    void executeCommandLists( const STLW::Vector<ICommandList*>& lists, QueueType workingQueue, Fence& frameFence ) override;
    void waitForFrame( const Fence& frameFence, QueueType workingQueue ) override;
    void waitForQueue( QueueType workingQueue, QueueType dstQueue ) override;
    void queueWaitIdle( QueueType workingQueue, Fence& frameFence ) override;
    bool waitIdle() override;

    void oneTimeSubmit( std::function<void( ICommandList* cmd )>& commands ) override;

    bool          queryFeatureSupport( FeatureType feature, void* pInfo = nullptr, size_t infoSize = 0 ) const override;
    FormatSupport queryFormatSupport( Format format ) const override;
    API           getGraphicsAPI() override;

    NativeObject     getNativeObject( ObjectType objectType ) override;
    void             setDebugName( std::string_view name ) override;
    std::string_view getDebugName() const override;
    STLW::String     toString() const override;

    // Internal Queue Definition
    struct Queue {
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence>        fence;
        HANDLE                     fenceEvent = nullptr;
        u64                      fenceValue = 0;
    };

    const Queue* getQueue( const QueueType& type ) const;

    // Upload context for one time submits
    class UploadContext
    {
    public:
        void init( const ComPtr<ID3D12Device2>& device );
        void oneTimeSubmitRaw( const Memory::OwnerPtr<Queue>& uploadQueue, const std::function<void( const ComPtr<ID3D12GraphicsCommandList>& )>& commands );
        void oneTimeSubmit( const Memory::OwnerPtr<Queue>& uploadQueue, const std::function<void( ICommandList* )>& commands );

    private:
        CommandListOwnerPtr _cmdList = nullptr;
        ComPtr<ID3D12Fence> _fence;
        HANDLE              _fenceEvent = nullptr;
        u64               _fenceValue = 0;
        mutable std::mutex  _mutex;
    };
    // Graphics API Context
    struct Context {

        ComPtr<IDXGIAdapter4> adapter;
        ComPtr<ID3D12Device2> device;

        // VM Allocator
        ComPtr<D3D12MA::Allocator> allocator;

        // Command Queues
        Memory::OwnerPtr<Queue> primaryQueue;
        Memory::OwnerPtr<Queue> computeQueue;
        Memory::OwnerPtr<Queue> copyQueue;

        // CPU Only Heaps
        DX12DescriptorHeap heapSRV;
        DX12DescriptorHeap heapRTV;
        DX12DescriptorHeap heapDSV;
        DX12DescriptorHeap heapUAV;
        DX12DescriptorHeap heapSamplers;

        UploadContext uploadContext = {};
    };

private:
    struct ExtensionSupportInfo {
        bool nvapiIsInitialized                 = false;
        bool singlePassStereoSupported          = false;
        bool hlslExtensionsSupported            = false;
        bool fastGeometryShaderSupported        = false;
        bool rayTracingSupported                = false;
        bool traceRayInlineSupported            = false;
        bool meshletsSupported                  = false;
        bool variableRateShadingSupported       = false;
        bool opacityMicromapSupported           = false;
        bool rayTracingClustersSupported        = false;
        bool linearSweptSpheresSupported        = false;
        bool spheresSupported                   = false;
        bool shaderExecutionReorderingSupported = false;
        bool samplerFeedbackSupported           = false;
        bool aftermathEnabled                   = false;
        bool heapDirectlyIndexedEnabled         = false;
        bool coopVecInferencingSupported        = false;
        bool coopVecTrainingSupported           = false;
    };
    struct FeatureData {
        ComPtr<ID3D12Device2> device2;
        ComPtr<ID3D12Device5> device5;
        ComPtr<ID3D12Device8> device8;

        D3D12_FEATURE_DATA_D3D12_OPTIONS  options  = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    };

    ComPtr<IDXGIAdapter4> getGPUAdapter( u32 preferredDeviceID );
    ComPtr<ID3D12Device2> createDevice( const ComPtr<IDXGIAdapter4>& gpuAdapter );
    void                  enableDebugLayer();
    void                  checkExtensions() override;

    Memory::OwnerPtr<Queue> createCommandQueue( const QueueType& type, const std::string& name );

    Queue* getQueueRW( const QueueType& type );

    IDX12Device::Description _desc;
    Context                  _ctx;

    ExtensionSupportInfo _ext;
    FeatureData          _featureData;

    bool     _initialized    = false;
    String64 _gpuAdapterName = "Unknown Device";
};

} // namespace Graphics::RHI
AXION_NAMESPACE_END