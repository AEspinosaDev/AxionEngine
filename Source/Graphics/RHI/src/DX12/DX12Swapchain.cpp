#include "DX12Swapchain.hpp"
#include "DX12Debug.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DX12Swapchain::DX12Swapchain( const HWND                     hwnd,
                              DX12Device::Context&           ctx,
                              const ISwapchain::Description& desc )
    : _device( ctx.device )
    , _desc( desc ) {
    // DXGI Factory
    ComPtr<IDXGIFactory4> dxgiFactory4;
#ifdef AXION_DEBUG
    UINT createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#else
    UINT createFactoryFlags = 0;
#endif
    DX_CHECK( CreateDXGIFactory2( createFactoryFlags, IID_PPV_ARGS( &dxgiFactory4 ) ) );

    // Swapchain desc
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width                 = desc.size.width;
    swapChainDesc.Height                = desc.size.height;
    swapChainDesc.Format                = DX12Translator::get( _desc.format );
    swapChainDesc.BufferCount           = desc.imageCount;
    swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
    swapChainDesc.SampleDesc.Count      = 1;
    swapChainDesc.SampleDesc.Quality    = 0;
    _desc.tearingSupported              = checkTearingSupport();
    swapChainDesc.Flags                 = _desc.tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    // Create swapchain
    ComPtr<IDXGISwapChain1> swapChain1;
    DX_CHECK( dxgiFactory4->CreateSwapChainForHwnd(
        ctx.primaryQueue->queue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1 ) );

    DX_CHECK( dxgiFactory4->MakeWindowAssociation( hwnd, DXGI_MWA_NO_ALT_ENTER ) );
    DX_CHECK( swapChain1.As( &_swapchain ) );

    // Descriptor heap
    _heapRTV.init( _device.Get(), DX12DescriptorHeap::Type::RTV, _desc.imageCount );

    // Create backbuffers
    updateImages();

    AXION_LOG_INFO( Logger::Module::RHI, "DX12 Swapchain created [{}]", _desc.debugName );
}

uint32_t DX12Swapchain::getCurrentImageIndex() {
    return _currentImage;
}

const ISwapchain::Description& DX12Swapchain::getDescription() {
    return _desc;
}

const std::vector<TexturePtr>& DX12Swapchain::getSwapImages() {
    return _swapImages;
}

void DX12Swapchain::update( const Description& newDesc ) {
    // Check if any relevant properties changed
    bool needsResize =
        ( _desc.size.width != newDesc.size.width ) ||
        ( _desc.size.height != newDesc.size.height ) ||
        ( _desc.format != newDesc.format );

    if ( !needsResize )
        return; // No need to do anything

    DX_CHECK( _device->GetDeviceRemovedReason() );

    // Release references to old back buffers before resizing
    _swapImages.clear();
    _heapRTV.reset();

    _desc = newDesc;

    DXGI_FORMAT newFormat = DX12Translator::get( _desc.format );
    DX_CHECK( _swapchain->ResizeBuffers(
        _desc.imageCount,
        _desc.size.width,
        _desc.size.height,
        newFormat,
        _desc.tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0 ) );

    // // Recreate RTV descriptor heap
    // _heapRTV.init( _device, DX12DescriptorHeap::Type::RTV, _desc.imageCount );
    // Recreate backbuffer textures and their RTVs
    updateImages();

    AXION_LOG_INFO( Logger::Module::RHI,
                    "DX12 Swapchain Updated: {}x{}, format {}",
                    _desc.size.width,
                    _desc.size.height,
                    (int)_desc.format );
}

uint32_t DX12Swapchain::acquireNextImage() {
    _currentImage = _swapchain->GetCurrentBackBufferIndex();
    return _currentImage;
}
DX12Swapchain::~DX12Swapchain() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying DX12 Swapchain [{}]", _desc.debugName );
}
void DX12Swapchain::updateImages() {

    _swapImages.resize( _desc.imageCount, nullptr );
    TextureDesc desc = {
        .size   = _desc.size.to3D(),
        .format = _desc.format,

        .debugName = "Swapchain Backbuffer",
        .viewFlags = TextureViewRenderTarget };
    DX12Device::Context ctx = { .device = _device, .heapRTV = _heapRTV };
    for ( uint i = 0; i < _desc.imageCount; ++i )
    {
        ComPtr<ID3D12Resource> backBuffer;
        DX_CHECK( _swapchain->GetBuffer( i, IID_PPV_ARGS( &backBuffer ) ) );

        DX12Texture* rawBackBufferTexture = new DX12Texture( backBuffer, desc, ctx, false );
        rawBackBufferTexture->stateTracker().setState( ResourceState::Present );
        TexturePtr backBufferTexture;
        backBufferTexture.attach( rawBackBufferTexture );
        // Assign to vector (old element will be released automatically)
        // _swapImages[i] = std::move( backBufferTexture );
        _swapImages[i] = backBufferTexture;
    }
} // namespace RHI
void DX12Swapchain::present() {
    UINT syncInterval = _desc.presentMode == PresentMode::Vsync ? 1 : 0;
    UINT presentFlags = _desc.tearingSupported && _desc.presentMode != PresentMode::Vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    DX_CHECK( _swapchain->Present( syncInterval, presentFlags ) );
}

bool DX12Swapchain::checkTearingSupport() {
    BOOL allowTearing = FALSE;

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
    // graphics debugging tools which will not support the 1.5 factory interface
    // until a future update.
    ComPtr<IDXGIFactory4> factory4;
    if ( SUCCEEDED( CreateDXGIFactory1( IID_PPV_ARGS( &factory4 ) ) ) )
    {
        ComPtr<IDXGIFactory5> factory5;
        if ( SUCCEEDED( factory4.As( &factory5 ) ) )
        {
            if ( FAILED( factory5->CheckFeatureSupport(
                     DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                     &allowTearing,
                     sizeof( allowTearing ) ) ) )
            {
                allowTearing = FALSE;
            }
        }
    }

    return allowTearing == TRUE;
}

NativeObject DX12Swapchain::getNativeObject( ObjectType objectType ) {
    switch ( objectType )
    {
        case ObjectTypes::DX12_SwapChain:
            return NativeObject( objectType, _swapchain.Get() );
        default:
            AXION_LOG_ERROR( Logger::Module::RHI, "DX12 Swapchain | Wrong Object Type" );
            return nullptr;
    }
}

void DX12Swapchain::setDebugName( const std::string& name ) {
    _desc.debugName = name;
    // _swapchain->se( std::wstring( name.begin(), name.end() ).c_str() );
}

const std::string& DX12Swapchain::getDebugName() const {
    return _desc.debugName;
}

std::string RHI::DX12Swapchain::toString() const {
    return std::string();
}

} // namespace Graphics::RHI
AXION_NAMESPACE_END