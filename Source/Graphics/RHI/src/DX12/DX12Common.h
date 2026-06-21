
#pragma once
#include "Axion/Common/Containers/String.h"

// DirectX 12
#include <wrl/client.h>
using namespace Microsoft::WRL;

#include <directx/d3dx12.h> // D3D12 extension library.

#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#pragma comment( lib, "dxguid.lib" )

#include <D3D12MemAlloc.h>

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

constexpr u32 D3D12_DESCRIPTOR_TYPE_COUNT   = 4; // THIs needs to be in parity with microsoft D3D12 descriptor types (CBV, SRV, UAV, Sampler)


// Internal hlsl descriptor mapping for DX12
struct D3D12BindingMapping {
    u32 hlslBase;
    u32 count;
    u32 globalOffset; // Offset in the global descriptor heap (for DX12)
};

struct D3D12BindingMappingLUT {
    // Per register offsets: t, u, s, b
    FixedArray<SmallVector<D3D12BindingMapping, 4>, D3D12_DESCRIPTOR_TYPE_COUNT> registerMappings;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END

/***
 * Utility function to set native debug names on D3D12 objects.
 * Converts UTF-8 string to wide string and calls SetName on the object.
 */
static void setNativeName( ID3D12Object* obj, Axion::StringView n ) {
    if ( !obj )
        return;
    wchar_t wname[128];
    int     result = MultiByteToWideChar( CP_UTF8, 0, n.data(), (int)n.length(), wname, 127 );
    if ( result > 0 )
    {
        wname[result] = L'\0';
        obj->SetName( wname );
    }
};