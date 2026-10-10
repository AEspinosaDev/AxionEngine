#pragma once
#include "Axion/Graphics/RHI/IGUIBackend.h"
#include "DX12Common.h"
#include "DX12Device.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12GUIBackend final : public IGUIBackend
{
public:
    DX12GUIBackend( DX12Device::Context& ctx, const GUIBackendDesc& desc );
    ~DX12GUIBackend() override;

    const GUIBackendDesc& getDescription() const { return _desc; }

    void newFrame() const override;
    void render( ICommandList* cmd ) const override;

private:
    DX12DescriptorHeap&      _gpuHeapSRV;
    DescriptorHeapAllocation _fontSlot; ///< Font atlas SRV inside the device global heap
    GUIBackendDesc           _desc;

    void ( *_platformNewFrame )() = nullptr;
    void ( *_platformShutdown )() = nullptr;
};



} // namespace Graphics::RHI

AXION_NAMESPACE_END
