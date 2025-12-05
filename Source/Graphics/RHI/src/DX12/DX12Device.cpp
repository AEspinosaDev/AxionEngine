#include "DX12Device.hpp"
#include "DX12CommandList.hpp"
#include "DX12Debug.hpp"
#include "DX12Pipeline.hpp"
#include "DX12Resource.hpp"
#include "DX12Swapchain.hpp"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DX12DevicePtr RHI::createDX12Device( const DX12DeviceDesc& desc ) {
    DX12Device*   raw = new DX12Device( desc );
    DX12DevicePtr dev;
    dev.attach( raw );
    return dev;
}

DX12Device::DX12Device( const IDX12Device::Description& desc ) {

    _desc = desc;

    if ( desc.enableDebugLayer )
        enableDebugLayer();

    // Initializing Context
    {

        _ctx.adapter = getGPUAdapter();

        _ctx.device = createDevice( _ctx.adapter );
        _ctx.device->SetName( std::wstring( desc.debugName.begin(), desc.debugName.end() ).c_str() );
        checkExtensions();

        _ctx.primaryQueue = createCommandQueue( QueueType::Graphics, "Graphics Queue" );
        _ctx.computeQueue = createCommandQueue( QueueType::Compute, "Compute Queue" );
        _ctx.copyQueue    = createCommandQueue( QueueType::Transfer, "Copy Queue" );

        _ctx.heapSRV.init( _ctx.device.Get(), DX12DescriptorHeap::Type::CBV_SRV_UAV, desc.shaderResourceViewHeapSize );
        _ctx.heapRTV.init( _ctx.device.Get(), DX12DescriptorHeap::Type::RTV, desc.renderTargetViewHeapSize );
        _ctx.heapDSV.init( _ctx.device.Get(), DX12DescriptorHeap::Type::DSV, desc.depthStencilViewHeapSize );
        _ctx.heapSamplers.init( _ctx.device.Get(), DX12DescriptorHeap::Type::Sampler, desc.samplerHeapSize );

        _ctx.uploadContext.init( _ctx.device );
    }

    _initialized = true;

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Device [{}] created", _desc.debugName );
    AXION_LOG_INFO( Logger::Module::RHI, toString() );
}

DX12Device::~DX12Device() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Device [{}]", _desc.debugName );
}

SwapchainPtr DX12Device::createSwapchain( const NativeObject& Ptr, const SwapchainDesc& desc ) {
    HWND hwnd = nullptr;
    switch ( Ptr.integer )
    {
        case ObjectTypes::WIN32_WINDOW:
#ifdef _WIN32
            hwnd = Ptr;
#endif
            break;
        case ObjectTypes::GLFW_Window:
            hwnd = glfwGetWin32Window( Ptr );
            break;
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "Unsupported platform for swapchain" );
            throw AxionException( "Unsupported platform for swapchain" );
    }
    DX12Swapchain* raw = new DX12Swapchain( hwnd, _ctx, desc );
    SwapchainPtr   swp;
    swp.attach( raw );
    return swp;
}

CommandListPtr DX12Device::createCommandList( const CommandListDesc& desc ) {
    DX12CommandList* raw = new DX12CommandList( _ctx.device, desc );
    CommandListPtr   cmd;
    cmd.attach( raw );
    return cmd;
}

TexturePtr DX12Device::createTexture( const TextureDesc& desc, const void* initialData ) {
    DX12Texture* raw = new DX12Texture( desc, _ctx, initialData );
    TexturePtr   tex;
    tex.attach( raw );
    return tex;
}

BufferPtr RHI::DX12Device::createBuffer( const BufferDesc& desc, const void* initialData ) {
    DX12Buffer* raw = new DX12Buffer( desc, _ctx, initialData );
    BufferPtr   buff;
    buff.attach( raw );
    return buff;
}

SamplerPtr DX12Device::createSampler( const SamplerDesc& desc ) {
    DX12Sampler* raw = new DX12Sampler( desc, _ctx );
    SamplerPtr   sampler;
    sampler.attach( raw );
    return sampler;
}

PipelineLayoutPtr DX12Device::createPipelineLayout( const PipelineLayoutDesc& desc ) {
    DX12PipelineLayout* raw = new DX12PipelineLayout( _ctx.device, desc );
    PipelineLayoutPtr   layout;
    layout.attach( raw );
    return layout;
}

GraphicPipelinePtr DX12Device::createGraphicPipeline( const GraphicPipelineDesc& desc ) {
    DX12GraphicPipeline* raw = new DX12GraphicPipeline( _ctx.device, desc );
    GraphicPipelinePtr   pip;
    pip.attach( raw );
    return pip;
}

ComputePipelinePtr DX12Device::createComputePipeline( const ComputePipelineDesc& desc ) {
    DX12ComputePipeline* raw = new DX12ComputePipeline( _ctx.device, desc );
    ComputePipelinePtr   pip;
    pip.attach( raw );
    return pip;
}

DescriptorAllocatorPtr DX12Device::createDescriptorAllocator( const DescriptorAllocatorDesc& desc ) {
    DX12DescriptorAllocator* raw = new DX12DescriptorAllocator( _ctx.device.Get(), desc );
    DescriptorAllocatorPtr   dAlloc;
    dAlloc.attach( raw );
    return dAlloc;
}

void DX12Device::executeCommandLists( const std::vector<ICommandList*>& lists, QueueType workingQueue, Fence& frameFence ) {
    std::vector<ID3D12CommandList*> nativeLists;
    nativeLists.reserve( lists.size() );
    for ( ICommandList* list : lists )
    {
        ID3D12CommandList* nativeList = list->getNativeObject( ObjectTypes::DX12_CommandList );
        nativeLists.push_back( nativeList );
    }

    auto queue = getQueue( workingQueue );
    queue->queue->ExecuteCommandLists( static_cast<UINT>( nativeLists.size() ), nativeLists.data() );

    // Signal fence for this frame
    ++queue->fenceValue;

    DX_CHECK( queue->queue->Signal( queue->fence.Get(), queue->fenceValue ) );

    frameFence.value = queue->fenceValue;
}

void DX12Device::waitForFrame( const Fence& frameFence, QueueType workingQueue ) {
    auto queue = getQueue( workingQueue );
    if ( queue->fence->GetCompletedValue() < frameFence.value )
    {
        DX_CHECK( queue->fence->SetEventOnCompletion( frameFence.value, queue->fenceEvent ) );
        WaitForSingleObject( queue->fenceEvent, INFINITE );
    }
}

void DX12Device::waitForQueue( QueueType workingQueue, QueueType dstQueue ) {
    auto odstQueue = getQueue( dstQueue );
    auto srcQueue  = getQueue( workingQueue );
    DX_CHECK( odstQueue->queue->Wait( srcQueue->fence.Get(), srcQueue->fenceValue ) );
}

void DX12Device::queueWaitIdle( QueueType workingQueue, Fence& frameFence ) {
    auto q = getQueue( workingQueue );
    // ulong fenceValueForSignal = ++frameFence.value;
    q->fenceValue++;
    DX_CHECK( q->queue->Signal( q->fence.Get(), q->fenceValue ) );

    frameFence.value = q->fenceValue;

    if ( q->fence->GetCompletedValue() < q->fenceValue )
    {
        DX_CHECK( q->fence->SetEventOnCompletion( q->fenceValue, q->fenceEvent ) );
        WaitForSingleObject( q->fenceEvent, INFINITE );
    }
}

bool DX12Device::waitIdle() {
    // Wait for each queue individually
    for ( QueueType type : { QueueType::Graphics, QueueType::Compute, QueueType::Transfer } )
    {
        auto q = getQueue( type );
        if ( !q )
            continue;

        Fence tempFence;
        tempFence.value = q->fenceValue; // start from the current value

        queueWaitIdle( type, tempFence );
    }
    return true;
}

bool DX12Device::queryFeatureSupport( Feature feature, void* pInfo, size_t infoSize ) const {
    switch ( feature ) // NOLINT(clang-diagnostic-switch-enum)
    {
        case Feature::DeferredCommandLists:
            return true;
        case Feature::SinglePassStereo:
            return _ext.singlePassStereoSupported;
        case Feature::RayTracingAccelStruct:
            return _ext.rayTracingSupported;
        case Feature::RayTracingPipeline:
            return _ext.rayTracingSupported;
        case Feature::RayTracingOpacityMicromap:
            return _ext.opacityMicromapSupported;
        case Feature::RayTracingClusters:
            return _ext.rayTracingClustersSupported;
        case Feature::RayQuery:
            return _ext.traceRayInlineSupported;
        case Feature::FastGeometryShader:
            return _ext.fastGeometryShaderSupported;
        case Feature::ShaderExecutionReordering:
            return _ext.shaderExecutionReorderingSupported;
        case Feature::Spheres:
            return _ext.spheresSupported;
        case Feature::LinearSweptSpheres:
            return _ext.linearSweptSpheresSupported;
        case Feature::Meshlets:
            return _ext.meshletsSupported;
            //    *******************************MORE
        default:
            return false;
    }
}

FormatSupport DX12Device::queryFormatSupport( Format format ) const {
    // WIP
    return FormatSupport::None;
}

ComPtr<IDXGIAdapter4> DX12Device::getGPUAdapter() {

    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT                  createFactoryFlags = 0;
    if ( _desc.enableDebugLayer )
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

    DX_CHECK( CreateDXGIFactory2( createFactoryFlags, IID_PPV_ARGS( &dxgiFactory ) ) );

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if ( _desc.useWarp )
    {
        DX_CHECK( dxgiFactory->EnumWarpAdapter( IID_PPV_ARGS( &dxgiAdapter1 ) ) );
        DX_CHECK( dxgiAdapter1.As( &dxgiAdapter4 ) );
    } else
    {
        SIZE_T maxDedicatedVideoMemory = 0;
        for ( UINT i = 0; dxgiFactory->EnumAdapters1( i, &dxgiAdapter1 ) != DXGI_ERROR_NOT_FOUND; ++i )
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1( &dxgiAdapterDesc1 );

            // Check to see if the adapter can create a D3D12 device without actually
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if ( ( dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) == 0 &&
                 SUCCEEDED( D3D12CreateDevice( dxgiAdapter1.Get(),
                                               D3D_FEATURE_LEVEL_11_0,
                                               __uuidof( ID3D12Device ),
                                               nullptr ) ) &&
                 dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory )
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                DX_CHECK( dxgiAdapter1.As( &dxgiAdapter4 ) );
            }
        }
    }

    return dxgiAdapter4;
}

ComPtr<ID3D12Device2> DX12Device::createDevice( const ComPtr<IDXGIAdapter4>& gpuAdapter ) {

    ComPtr<ID3D12Device2> device;
    DX_CHECK( D3D12CreateDevice( gpuAdapter.Get(), static_cast<D3D_FEATURE_LEVEL>( _desc.featureLevel ), IID_PPV_ARGS( &device ) ) );

    if ( !_desc.enableDebugLayer )
    {
        ComPtr<ID3D12InfoQueue> pInfoQueue;
        if ( SUCCEEDED( _ctx.device.As( &pInfoQueue ) ) )
        {
            pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE );
            pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE );
            pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE );
        }
        // Suppress whole categories of messages
        // D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
            {
                D3D12_MESSAGE_SEVERITY_INFO };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                     // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        // NewFilter.DenyList.NumCategories = _countof(Categories);
        // NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof( Severities );
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs        = _countof( DenyIds );
        NewFilter.DenyList.pIDList       = DenyIds;

        DX_CHECK( pInfoQueue->PushStorageFilter( &NewFilter ) );
    }

    return device;
}

void DX12Device::enableDebugLayer() {
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    ComPtr<ID3D12Debug> debugInterface;
    DX_CHECK( D3D12GetDebugInterface( IID_PPV_ARGS( &debugInterface ) ) );
    debugInterface->EnableDebugLayer();
}

void RHI::DX12Device::checkExtensions() {

    _ctx.device->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS, &_featureData.options, sizeof( _featureData.options ) );
    _ctx.device->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS1, &_featureData.options1, sizeof( _featureData.options1 ) );
    bool hasOptions5 = SUCCEEDED( _ctx.device->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS5, &_featureData.options5, sizeof( _featureData.options5 ) ) );
    bool hasOptions6 = SUCCEEDED( _ctx.device->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS6, &_featureData.options6, sizeof( _featureData.options6 ) ) );
    bool hasOptions7 = SUCCEEDED( _ctx.device->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS7, &_featureData.options7, sizeof( _featureData.options7 ) ) );

    if ( SUCCEEDED( _ctx.device->QueryInterface( IID_PPV_ARGS( &_featureData.device5 ) ) ) && hasOptions5 )
    {
        _ext.rayTracingSupported     = _featureData.options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
        _ext.traceRayInlineSupported = _featureData.options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1;

        // m_OpacityMicromapSupported = _featureData.options 5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_2;
    }

    if ( SUCCEEDED( _ctx.device->QueryInterface( IID_PPV_ARGS( &_featureData.device2 ) ) ) && hasOptions7 )
    {
        _ext.meshletsSupported = _featureData.options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
    }

    if ( SUCCEEDED( _ctx.device->QueryInterface( IID_PPV_ARGS( &_featureData.device8 ) ) ) && hasOptions7 )
    {
        _ext.samplerFeedbackSupported = _featureData.options7.SamplerFeedbackTier >= D3D12_SAMPLER_FEEDBACK_TIER_0_9;
    }

    if ( hasOptions6 )
    {
        _ext.variableRateShadingSupported = _featureData.options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_2;
    }

    {
        D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
        D3D12_COMMAND_SIGNATURE_DESC csDesc  = {};
        csDesc.NumArgumentDescs              = 1;
        csDesc.pArgumentDescs                = &argDesc;

        csDesc.ByteStride = 16;
        argDesc.Type      = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        _ctx.device->CreateCommandSignature( &csDesc, nullptr, IID_PPV_ARGS( &_ctx.drawIndirectSignature ) );

        csDesc.ByteStride = 20;
        argDesc.Type      = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        _ctx.device->CreateCommandSignature( &csDesc, nullptr, IID_PPV_ARGS( &_ctx.drawIndexedIndirectSignature ) );

        csDesc.ByteStride = 12;
        argDesc.Type      = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        _ctx.device->CreateCommandSignature( &csDesc, nullptr, IID_PPV_ARGS( &_ctx.dispatchIndirectSignature ) );
    }

    if ( _desc.enableHeapDirectlyIndexed )
    {
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel    = { D3D_SHADER_MODEL_6_6 };
        bool                            hasShaderModel = SUCCEEDED( _ctx.device->CheckFeatureSupport( D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof( shaderModel ) ) );

        _ext.heapDirectlyIndexedEnabled = _featureData.options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3 &&
                                          hasShaderModel && shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6;
    }
}

API DX12Device::getGraphicsAPI() {
    return API::DirectX12;
}

std::unique_ptr<DX12Device::Queue> DX12Device::createCommandQueue( const QueueType& type, const std::string& name ) {
    auto q = NEW_U( DX12Device::Queue )();

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type                     = DX12Translator::get( type );
    desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask                 = 0;

    DX_CHECK( _ctx.device->CreateCommandQueue( &desc, IID_PPV_ARGS( &q->queue ) ) );

    DX_CHECK( _ctx.device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS( &q->fence ) ) );

    q->fenceEvent = ::CreateEvent( nullptr, FALSE, FALSE, nullptr );
    AXION_LOG_ASSERT( q->fenceEvent, Logger::Module::RHI, "Failed to create queue fence event." );

    q->fenceValue = 0;

    q->queue->SetName( std::wstring( name.begin(), name.end() ).c_str() );
    std::string fenceName = name + " Fence";
    q->fence->SetName( std::wstring( fenceName.begin(), fenceName.end() ).c_str() );

    return q;
}

DX12Device::Queue* RHI::DX12Device::getQueue( const QueueType& type ) {
    switch ( type )
    {
        case QueueType::Graphics:
            return _ctx.primaryQueue.get();
        case QueueType::Compute:
            return _ctx.computeQueue.get();
        case QueueType::Transfer:
            return _ctx.copyQueue.get();
        default:
            return nullptr;
    }
}

NativeObject DX12Device::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_Device:
            return NativeObject( objectType, _ctx.device.Get() );
        // case ObjectTypes::DX12_CommandQueue:
        //     return Object( getQueue( CommandQueue::Graphics )->queue.Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Device | Wrong Object Type" );
            return nullptr;
    }
}

void DX12Device::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    _ctx.device->SetName( std::wstring( name.begin(), name.end() ).c_str() );
}

const std::string& DX12Device::getDebugName() const {
    return _desc.debugName;
}

std::string RHI::DX12Device::toString() const {
    std::string deviceInfo = fmt::format(
        "DX12 Device Description:\n"
        "  Debug Name: {}\n"
        "  Feature Level: 0x{:X}\n"
        "  Enable Debug Layer: {}\n"
        "  Use WARP: {}\n"
        "  RTV Heap Size: {}\n"
        "  DSV Heap Size: {}\n"
        "  SRV Heap Size: {}\n"
        "  Sampler Heap Size: {}\n"
        "  Heap Directly Indexed: {}",
        _desc.debugName,
        static_cast<int>( _desc.featureLevel ),
        _desc.enableDebugLayer,
        _desc.useWarp,
        _desc.renderTargetViewHeapSize,
        _desc.depthStencilViewHeapSize,
        _desc.shaderResourceViewHeapSize,
        _desc.samplerHeapSize,
        _desc.enableHeapDirectlyIndexed );

    std::ostringstream ss;
    ss << "  Extensions:\n";
    ss << fmt::format( "    NVAPI Initialized: {}\n", _ext.nvapiIsInitialized );
    ss << fmt::format( "    Single Pass Stereo: {}\n", _ext.singlePassStereoSupported );
    ss << fmt::format( "    HLSL Extensions: {}\n", _ext.hlslExtensionsSupported );
    ss << fmt::format( "    Fast Geometry Shader: {}\n", _ext.fastGeometryShaderSupported );
    ss << fmt::format( "    Ray Tracing: {}\n", _ext.rayTracingSupported );
    ss << fmt::format( "    Trace Ray Inline: {}\n", _ext.traceRayInlineSupported );
    ss << fmt::format( "    Meshlets: {}\n", _ext.meshletsSupported );
    ss << fmt::format( "    Variable Rate Shading: {}\n", _ext.variableRateShadingSupported );
    ss << fmt::format( "    Opacity Micromap: {}\n", _ext.opacityMicromapSupported );
    ss << fmt::format( "    Ray Tracing Clusters: {}\n", _ext.rayTracingClustersSupported );
    ss << fmt::format( "    Linear Swept Spheres: {}\n", _ext.linearSweptSpheresSupported );
    ss << fmt::format( "    Spheres: {}\n", _ext.spheresSupported );
    ss << fmt::format( "    Shader Execution Reordering: {}\n", _ext.shaderExecutionReorderingSupported );
    ss << fmt::format( "    Sampler Feedback: {}\n", _ext.samplerFeedbackSupported );
    ss << fmt::format( "    Aftermath Enabled: {}\n", _ext.aftermathEnabled );
    ss << fmt::format( "    Heap Directly Indexed: {}\n", _ext.heapDirectlyIndexedEnabled );
    ss << fmt::format( "    Coop Vec Inferencing: {}\n", _ext.coopVecInferencingSupported );
    ss << fmt::format( "    Coop Vec Training: {}\n", _ext.coopVecTrainingSupported );

    return fmt::format( "{}\n\n{}", deviceInfo, ss.str() );
}

void DX12Device::UploadContext::init( const ComPtr<ID3D12Device2>& device ) {

    auto dx12type = DX12Translator::get( QueueType::Graphics );

    DX_CHECK( device->CreateCommandAllocator( dx12type, IID_PPV_ARGS( &_cmdAllocator ) ) );

    DX_CHECK( device->CreateCommandList( 0, dx12type, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS( &_cmdList ) ) );

    DX_CHECK( _cmdList->Close() );

    // Create fence
    DX_CHECK( device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &_fence ) ) );
    _fenceValue = 1;

    // Create OS event for GPU sync
    _fenceEvent = ::CreateEvent( nullptr, FALSE, FALSE, nullptr );
}

void DX12Device::UploadContext::oneTimeSubmit( const std::unique_ptr<Queue>& uploadQueue, const std::function<void( const ComPtr<ID3D12GraphicsCommandList>& )>& commands ) {

    DX_CHECK( _cmdAllocator->Reset() );
    DX_CHECK( _cmdList->Reset( _cmdAllocator.Get(), nullptr ) );
    commands( _cmdList );
    DX_CHECK( _cmdList->Close() );

    // Submit to queue
    ID3D12CommandList* lists[] = { _cmdList.Get() };
    uploadQueue->queue->ExecuteCommandLists( 1, lists );

    // Fence
    _fenceValue++;
    DX_CHECK( uploadQueue->queue->Signal( _fence.Get(), _fenceValue ) );

    if ( _fence->GetCompletedValue() < _fenceValue )
    {
        DX_CHECK( _fence->SetEventOnCompletion( _fenceValue, _fenceEvent ) );
        WaitForSingleObject( _fenceEvent, INFINITE );
    }
}

} // namespace Graphics::RHI
AXION_NAMESPACE_END