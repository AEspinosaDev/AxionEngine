#include "StateTracking.h"
#include <algorithm>

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

ResourceStateTracker::ResourceStateTracker( u32 mipLevels, u32 arrayLayers )
    : _miplevels( mipLevels )
    , _arrayLayers( arrayLayers ) {
}
bool ResourceStateTracker::needsTransition( ResourceState newState ) const {
    if ( !_initialized )
        return false;
    return _globalState != newState;
}

void ResourceStateTracker::setState( ResourceState newState ) {
    _globalState = newState;
    _initialized = true;
}

void ResourceStateTracker::reset() {
    _globalState = ResourceState::Undefined;
    _initialized = false;
    _subresourceStates.clear();
}

void ResourceStateTracker::setState( ResourceState newState, u32 mip, u32 layer ) {
    if ( _subresourceStates.isEmpty() )
        _subresourceStates.resize( _miplevels * _arrayLayers, ResourceState::Undefined );

    uint32_t idx            = layer * _miplevels + mip;
    _subresourceStates[idx] = newState;
}

ResourceState ResourceStateTracker::getState( u32 mip, u32 layer ) const {
    if ( _subresourceStates.isEmpty() )
        return _globalState;

    uint32_t idx = layer * _miplevels + mip;
    return _subresourceStates[idx];
}

} // namespace RHI
AXION_NAMESPACE_END