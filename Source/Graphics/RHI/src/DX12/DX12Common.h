
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