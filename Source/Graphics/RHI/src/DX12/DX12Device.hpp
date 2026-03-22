#pragma once
#include "Axion/Graphics/RHI/DX12.h"
#include "DX12Descriptor.h"
#include <functional>

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12Device final : public RefCounter<IDX12Device>
{

public:
    DX12Device( const IDX12Device::Description& desc );
    ~DX12Device() override;

    SwapchainPtr          createSwapchain( const NativeObject& Ptr, const SwapchainDesc& desc = {} ) override;
    CommandListPtr        createCommandList( const CommandListDesc& desc ) override;
    TexturePtr            createTexture( const TextureDesc& desc, const void* initialData = nullptr ) override;
    BufferPtr             createBuffer( const BufferDesc& desc, const void* initialData = nullptr ) override;
    SamplerPtr            createSampler( const SamplerDesc& desc ) override;
    AccelPtr              createAccel( const AccelDesc& desc, bool immediateBuild = false ) override;
    PipelineLayoutPtr     createPipelineLayout( const PipelineLayoutDesc& desc ) override;
    GraphicPipelinePtr    createGraphicPipeline( const GraphicPipelineDesc& desc ) override;
    ComputePipelinePtr    createComputePipeline( const ComputePipelineDesc& desc ) override;
    RayTracingPipelinePtr createRayTracingPipeline( const RayTracingPipelineDesc& desc ) override;
    MeshPipelinePtr       createMeshPipeline( const MeshPipelineDesc& desc ) override;

    DescriptorAllocatorPtr createDescriptorAllocator( const DescriptorAllocatorDesc& desc ) override;
    SBTAllocatorPtr        createSBTAllocator( const SBTAllocatorDesc& desc ) override;
    TransientAllocatorPtr  createTransientAllocator( const TransientAllocatorDesc& desc ) override;

    void executeCommandLists( const std::vector<ICommandList*>& lists, QueueType workingQueue, Fence& frameFence ) override;
    void waitForFrame( const Fence& frameFence, QueueType workingQueue ) override;
    void waitForQueue( QueueType workingQueue, QueueType dstQueue ) override;
    void queueWaitIdle( QueueType workingQueue, Fence& frameFence ) override;
    bool waitIdle() override;

    void oneTimeSubmit( std::function<void( ICommandList* cmd )>& commands ) override;

    bool          queryFeatureSupport( Feature feature, void* pInfo = nullptr, size_t infoSize = 0 ) const override;
    FormatSupport queryFormatSupport( Format format ) const override;
    API           getGraphicsAPI() override;

    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( const std::string& name ) override;
    const std::string& getDebugName() const override;
    std::string        toString() const override;

    // Internal Queue Definition
    struct Queue {
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12Fence>        fence;
        HANDLE                     fenceEvent = nullptr;
        ulong                      fenceValue = 0;
    };

    const Queue* getQueue( const QueueType& type ) const;

    // Upload context for one time submits
    class UploadContext
    {
    public:
        void init( const ComPtr<ID3D12Device2>& device );
        void oneTimeSubmitRaw( const std::unique_ptr<Queue>& uploadQueue, const std::function<void( const ComPtr<ID3D12GraphicsCommandList>& )>& commands );
        void oneTimeSubmit( const std::unique_ptr<Queue>& uploadQueue, const std::function<void( ICommandList* )>& commands );

    private:
        CommandListPtr      _cmdList = nullptr;
        ComPtr<ID3D12Fence> _fence;
        HANDLE              _fenceEvent = nullptr;
        ulong               _fenceValue = 0;
        mutable std::mutex  _mutex;
    };
    // Graphics API Context
    struct Context {

        ComPtr<IDXGIAdapter4> adapter;
        ComPtr<ID3D12Device2> device;

        // VM Allocator
        ComPtr<D3D12MA::Allocator> allocator;

        // Command Queues
        std::unique_ptr<Queue> primaryQueue;
        std::unique_ptr<Queue> computeQueue;
        std::unique_ptr<Queue> copyQueue;

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

    ComPtr<IDXGIAdapter4> getGPUAdapter( uint preferredDeviceID ) override;
    ComPtr<ID3D12Device2> createDevice( const ComPtr<IDXGIAdapter4>& gpuAdapter ) override;
    void                  enableDebugLayer() override;
    void                  checkExtensions() override;

    std::unique_ptr<Queue> createCommandQueue( const QueueType& type, const std::string& name );

    Queue* getQueueRW( const QueueType& type );

    IDX12Device::Description _desc;
    Context                  _ctx;

    ExtensionSupportInfo _ext;
    FeatureData          _featureData;

    bool        _initialized    = false;
    std::string _gpuAdapterName = "Unknown Device";
};

} // namespace Graphics::RHI
AXION_NAMESPACE_END