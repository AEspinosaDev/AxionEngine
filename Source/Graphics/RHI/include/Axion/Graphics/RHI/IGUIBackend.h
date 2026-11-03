#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/ICommandList.h"
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>


AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_OWNER_PTR_FOR_TYPE( IGUIBackend, GUIBackend )

class IGUIBackend : public IDeviceObject
{
public:
    struct Description {
        PlatformType platform           = PlatformType::Win32;
        Format       backbufferFormat   = Format::RGBA8_UNORM;
        u32          framesInFlight     = 2;
        int          configFlags        = 0;
        void*        nativeWindowHandle = nullptr;
        String64     debugName          = "GUIBackend";
    };

    virtual ~IGUIBackend() = default;

    virtual const Description& getDescription() const = 0;

    // Prepare for a new frame
    virtual void newFrame() const = 0;
    // Dispatch draw commands into the provided command list
    virtual void render( ICommandList* cmd ) const = 0;
};

typedef IGUIBackend::Description GUIBackendDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END