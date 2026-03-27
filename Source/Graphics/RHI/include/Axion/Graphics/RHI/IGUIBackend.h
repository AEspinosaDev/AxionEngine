#pragma once
#include <Axion/Common/Memory/Ptr.h>
#include "Axion/Graphics/RHI/ICommandList.h"
#include "Axion/Graphics/RHI/IDevice.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_OWNER_PTR_FOR_TYPE( IGUIBackend, GUIBackend )

class IGUIBackend
{
public:
    struct Description {
        PlatformType platform           = PlatformType::Win32;
        Format       backbufferFormat   = Format::RGBA8_UNORM;
        uint         framesInFlight     = 2;
        int          configFlags        = 0;
        void*        nativeWindowHandle = nullptr;
    };

    virtual ~IGUIBackend() = default;

    // Prepare for a new frame
    virtual void newFrame() const = 0;
    // Dispatch draw commands into the provided command list
    virtual void render( ICommandList* cmd ) const = 0;
};

typedef IGUIBackend::Description GUIBackendDesc;

GUIBackendOwnerPtr createGUIBackendForDX12( IDevice* device, const GUIBackendDesc& desc );
GUIBackendOwnerPtr createGUIBackendForVulkan( IDevice* device, const GUIBackendDesc& desc );

} // namespace Graphics::RHI

AXION_NAMESPACE_END