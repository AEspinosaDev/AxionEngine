#pragma once
#include "Axion/Graphics/RHI/Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class ResourceStateTracker
{
public:
    explicit ResourceStateTracker( u32 mipLevels = 1, u32 arrayLayers = 1 );

    // --- Main API ---
    bool          needsTransition( ResourceState newState ) const;
    void          setState( ResourceState newState );
    void          reset(); // resets to Unknown/uninitialized
    ResourceState getCurrentState() const { return _globalState; }
    bool          isInitialized() const { return _initialized; }

    // --- Optional per-subresource (Textures) ---
    void          setState( ResourceState newState, u32 mip, u32 layer = 0 );
    ResourceState getState( u32 mip, u32 layer = 0 ) const;

private:
    ResourceState _globalState = ResourceState::Undefined;
    bool          _initialized = false;

    std::vector<ResourceState> _subresourceStates;
    u32                       _miplevels   = 1;
    u32                       _arrayLayers = 1;
};

} // namespace RHI
AXION_NAMESPACE_END
