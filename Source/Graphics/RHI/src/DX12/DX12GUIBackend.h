#pragma once
#include "Axion/Graphics/RHI/IGUIBackend.h"
#include "DX12Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12GUIBackend final : public IGUIBackend
{
public:
    DX12GUIBackend( ID3D12Device2* device, ID3D12CommandQueue* queue, const GUIBackendDesc& desc );
    ~DX12GUIBackend() override;

    const GUIBackendDesc& getDescription() const { return _desc; }

    void newFrame() const override;
    void render( ICommandList* cmd ) const override;

private:
    ComPtr<ID3D12DescriptorHeap> _guiHeap;
    GUIBackendDesc               _desc;

    void ( *_platformNewFrame )() = nullptr;
    void ( *_platformShutdown )() = nullptr;
};



} // namespace Graphics::RHI

AXION_NAMESPACE_END
