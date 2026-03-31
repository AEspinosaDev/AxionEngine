#pragma once
#include "Axion/Graphics/RHI/Memory.h"
#include "Axion/Graphics/RHI/ShaderBindingTable.h"
#include "DX12Resource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class DX12SBTAllocator : public ISBTAllocator
{
public:
    DX12SBTAllocator( const SBTAllocatorDesc& desc, DX12Device::Context& ctx );
    ~DX12SBTAllocator();

    SBT::View allocate( const ShaderBindingTable& sbt, IRayTracingPipeline* pip ) override;
    void      reset() override;

    const Description& getDescription() const override { return _desc; }
    NativeObject       getNativeObject( ObjectType objectType ) override;
    void               setDebugName( std::string_view name ) override;
    std::string_view   getDebugName() const override;
    STLW::String       toString() const override;

private:
    SBTAllocatorDesc _desc;

    Memory::OwnerPtr<DX12Buffer> _buffer = nullptr;
    LinearAllocator              _allocator;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END