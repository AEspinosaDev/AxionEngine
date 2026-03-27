#pragma once
#include "Axion/Graphics/RHI/IDevice.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( IDX12Device, DX12Device )

/// @brief DirectX 12 specific implementation of the Device interface.
/// Handles the initialization of the D3D12 backend, descriptor heaps, and adapter selection.
class IDX12Device : public IDevice
{

public:
    /// @brief Direct mapping of D3D_FEATURE_LEVEL hardware tiers.
    /// Specifies the minimum hardware capabilities required by the application.
    enum class FeatureLevel
    {
        _1_0_Generic = 0x100,
        _1_0_Core    = 0x1000,
        _9_1         = 0x9100,
        _9_2         = 0x9200,
        _9_3         = 0x9300,
        _10_0        = 0xa000,
        _10_1        = 0xa100,
        _11_0        = 0xb000,
        _11_1        = 0xb100,
        _12_0        = 0xc000,
        _12_1        = 0xc100,
        _12_2        = 0xc200
    };

    /// @brief Configuration structure for initializing the DX12 Backend.
    struct Description {
        uint         preferredDeviceID          = UINT32_MAX;          ///<  Index of the GPU adapter to use. Set to UINT32_MAX for auto-selection (best dedicated GPU).
        FeatureLevel featureLevel               = FeatureLevel::_12_1; ///< Minimum hardware feature level required.
        bool         enableDebugLayer           = true;                ///< Enables the D3D12 Debug Layer (validation errors/warnings). Recommended for debug builds.
        bool         useWarp                    = false;               ///< Forces the use of the WARP software rasterizer instead of hardware.
        std::string  debugName                  = "Device";
        uint         renderTargetViewHeapSize   = 1024;  ///< Capacity of the RTV Descriptor Heap.
        uint         depthStencilViewHeapSize   = 1024;  ///< Capacity of the DSV Descriptor Heap.
        uint         shaderResourceViewHeapSize = 16384; ///< Capacity of the CBV/SRV/UAV Descriptor Heap.
        uint         samplerHeapSize            = 1024;  ///< Capacity of the Sampler Descriptor Heap.
        ulong        vramBlockSize              = 0;     ///< Preferred VRAM block size for the global allocator. 0 lets the allocator choose the default (usually 64 MB). Set to 512ull * 1024 * 1024 for high-performance AAA scenarios.
        bool         enableHeapDirectlyIndexed  = false; ///< Enables SM 6.6 Dynamic Resources (Bindless) if hardware supports it.
    };

};

typedef IDX12Device::Description  DX12DeviceDesc;
typedef IDX12Device::FeatureLevel DX12DeviceFeatureLevel;

/// @brief Factory function to create a DirectX 12 Device instance.
DX12DevicePtr createDX12Device( const DX12DeviceDesc& desc );

} // namespace Graphics::RHI

AXION_NAMESPACE_END