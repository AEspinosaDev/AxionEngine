#pragma once
#include "VMemoryManager.h"
#include <Axion/Common/Memory/VMemoryArena.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

VMemoryArena::VMemoryArena( u32 capacity ) {
    _reservation = VMemoryManager::virtualReserve( capacity );
    _pageSize    = VMemoryManager::getPageSize();
}
VMemoryArena::~VMemoryArena() {
    if ( _reservation.isValid() )
    {
        VMemoryManager::virtualRelease( _reservation );
    }
}
VMemoryArena::VMemoryArena( VMemoryArena&& other ) noexcept
    : _reservation( other._reservation )
    , _pageSize( other._pageSize ) {
    other._reservation = {};
}

VMemoryArena& VMemoryArena::operator=( VMemoryArena&& other ) noexcept {
    if ( this != &other )
    {
        if ( _reservation.isValid() )
        {
            VMemoryManager::virtualRelease( _reservation );
        }
        _reservation       = other._reservation;
        _pageSize          = other._pageSize;
        other._reservation = {};
    }
    return *this;
}
bool VMemoryArena::commitRange( u32 offset, u32 size ) {
    if ( offset + size > _reservation.size )
    {
        AXION_LOG_ERROR( Logger::Module::Common, "VirtualArena OOM! Offset: {}, Size: {}, Capacity: {}", offset, size, _reservation.size );
        return false;
    }

    u32 pageAlignedOffset = ( offset / _pageSize ) * _pageSize;
    u32 pageAlignedEnd    = VMemoryManager::alignToPageSize( offset + size );
    u32 commitSize        = pageAlignedEnd - pageAlignedOffset;

    VMemoryView viewToCommit = {
        static_cast<byte*>( _reservation.ptr ) + pageAlignedOffset,
        commitSize };

    return VMemoryManager::virtualCommit( viewToCommit );
}
void VMemoryArena::decommitRange( u32 offset, u32 size ) {
    if ( size == 0 || offset + size > _reservation.size )
        return;

    u32 pageAlignedOffset = VMemoryManager::alignToPageSize( offset );

    u32 endOffset      = offset + size;
    u32 pageAlignedEnd = ( endOffset / _pageSize ) * _pageSize;

    if ( pageAlignedEnd <= pageAlignedOffset )
        return;

    u32 decommitSize = pageAlignedEnd - pageAlignedOffset;

    VMemoryView viewToDecommit = {
        static_cast<byte*>( _reservation.ptr ) + pageAlignedOffset,
        decommitSize };

    VMemoryManager::virtualDecommit( viewToDecommit );
}

} // namespace Memory

AXION_NAMESPACE_END