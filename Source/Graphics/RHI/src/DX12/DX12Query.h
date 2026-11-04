#pragma once
#include "Axion/Graphics/RHI/IQuery.h"
#include "DX12Device.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12TimeQueryPool : public ITimeQueryPool
{
public:
    DX12TimeQueryPool( const TimeQueryPoolDesc& desc, DX12Device::Context& ctx );
    ~DX12TimeQueryPool() override;

    const Description& getDescription() const override { return _desc; };

    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override { return _desc.debugName; };
    STLW::String toString() const override;

    u64  getFrequency() const override { return _frequency; }
    void readResults( u32 frame, u32 count, u64* outTicks ) override;

    ID3D12QueryHeap* getHeap() const { return _heap.Get(); }
    ID3D12Resource*  getReadbackBuffer() const { return _readback.Get(); }
    u32              getFrameBase( u32 frame ) const { return frame * _desc.capacity; }

private:
    TimeQueryPoolDesc _desc;

    ComPtr<ID3D12QueryHeap>     _heap;
    ComPtr<D3D12MA::Allocation> _allocation;
    ComPtr<ID3D12Resource>      _readback; ///< Always in COPY_DEST
    u64                         _frequency = 0;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END
