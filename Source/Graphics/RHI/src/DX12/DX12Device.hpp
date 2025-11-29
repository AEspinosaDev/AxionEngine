#pragma once
#include "Axion/Graphics/RHI/DX12.h"
#include "DX12DescriptorHeap.h"
#include <functional>

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12Device final : public RefCounter<IDX12Device>
{

public:
    DX12Device( const IDX12Device::Description& desc );
    ~DX12Device() override;

    SwapchainPtr       createSwapchain( const NativeObject& Ptr, const SwapchainDesc& desc = {} ) override;
    CommandListPtr     createCommandList( const CommandListDesc& desc ) override;
    TexturePtr         createTexture( const TextureDesc& desc, const void* initialData = nullptr ) override;
    BufferPtr          createBuffer( const BufferDesc& desc, const void* initialData = nullptr ) override;
    PipelineLayoutPtr  createPipelineLayout( const PipelineLayoutDesc& desc ) override;
    GraphicPipelinePtr createGraphicPipeline( const GraphicPipelineDesc& desc ) override;
    ComputePipelinePtr createComputePipeline( const ComputePipelineDesc& desc ) override;

    void executeCommandLists( const std::vector<ICommandList*>& lists, QueueType workingQueue, Fence& frameFence ) override;
    void waitForFrame( const Fence& frameFence, QueueType workingQueue ) override;
    void waitForQueue( QueueType workingQueue, QueueType dstQueue ) override;
    void queueWaitIdle( QueueType workingQueue, Fence& frameFence ) override;
    bool waitIdle() override;

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
    // Upload context for one time submits
    class UploadContext
    {
    public:
        void init( const ComPtr<ID3D12Device2>& device );
        void oneTimeSubmit( const std::unique_ptr<Queue>& uploadQueue, const std::function<void( const ComPtr<ID3D12GraphicsCommandList>& )>& commands );

    private:
        ComPtr<ID3D12GraphicsCommandList> _cmdList;
        ComPtr<ID3D12CommandAllocator>    _cmdAllocator;
        ComPtr<ID3D12Fence>               _fence;
        HANDLE                            _fenceEvent = nullptr;
        ulong                             _fenceValue = 0;
    };
    // Graphics API Context
    struct Context {

        ComPtr<IDXGIAdapter4> adapter;
        ComPtr<ID3D12Device2> device;

        std::unique_ptr<Queue> primaryQueue;
        std::unique_ptr<Queue> computeQueue;
        std::unique_ptr<Queue> copyQueue;

        DX12DescriptorHeap heapSRV;
        DX12DescriptorHeap heapRTV;
        DX12DescriptorHeap heapDSV;
        DX12DescriptorHeap heapUAV;

        UploadContext uploadContext = {};

        ComPtr<ID3D12CommandSignature> drawIndirectSignature;
        ComPtr<ID3D12CommandSignature> drawIndexedIndirectSignature;
        ComPtr<ID3D12CommandSignature> dispatchIndirectSignature;
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

    ComPtr<IDXGIAdapter4> getGPUAdapter() override;
    ComPtr<ID3D12Device2> createDevice( const ComPtr<IDXGIAdapter4>& gpuAdapter ) override;
    void                  enableDebugLayer() override;
    void                  checkExtensions() override;

    std::unique_ptr<Queue> createCommandQueue( const QueueType& type, const std::string& name );
    Queue*                 getQueue( const QueueType& type );

    IDX12Device::Description _desc;
    Context                  _ctx;

    ExtensionSupportInfo _ext;
    FeatureData          _featureData;

    bool _initialized = false;
};

} // namespace Graphics::RHI
AXION_NAMESPACE_END