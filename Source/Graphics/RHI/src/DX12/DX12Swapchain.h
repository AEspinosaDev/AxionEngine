#pragma once

#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/ISwapchain.h"
#include "DX12Device.h"
#include "DX12Resource.h"
#include "DX12TranslatorUnit.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_OWNER_PTR_FOR_TYPE( DX12Swapchain, DX12Swapchain )

class DX12Swapchain final : public ISwapchain
{
public:
    DX12Swapchain( const HWND hwnd, DX12Device::Context& ctx, const ISwapchain::Description& desc );
    ~DX12Swapchain() override;

    void                          updateImages() override;
    void                          present() override;
    u32                          acquireNextImage() override;
    u32                          getCurrentImageIndex() override;
    const Description&            getDescription() override;
    STLW::Vector<TextureOwnerPtr> releaseImages() override;
    void                          update( const Description& newDesc ) override;

    NativeObject     getNativeObject( ObjectType objectType ) override;
    void             setDebugName( std::string_view name ) override;
    std::string_view getDebugName() const override;
    STLW::String     toString() const override;

private:
    bool checkTearingSupport();

    ISwapchain::Description _desc;

    ComPtr<ID3D12Device2>   _device;
    ComPtr<IDXGISwapChain4> _swapchain;
    u32                    _currentImage;

    DX12DescriptorHeap            _heapRTV;
    STLW::Vector<TextureOwnerPtr> _swapImages;

    bool _initialized = false;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END