#pragma once
#include "VMemoryManager.h"
#include <Axion/Common/Memory/VMemoryArena.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

VMemoryArena::VMemoryArena( uint capacity ) {
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
bool VMemoryArena::commitRange( uint offset, uint size ) {
    if ( offset + size > _reservation.size )
    {
        AXION_LOG_ERROR( Logger::Module::Common, "VirtualArena OOM! Offset: {}, Size: {}, Capacity: {}", offset, size, _reservation.size );
        return false;
    }

    uint pageAlignedOffset = ( offset / _pageSize ) * _pageSize;
    uint pageAlignedEnd    = VMemoryManager::alignToPageSize( offset + size );
    uint commitSize        = pageAlignedEnd - pageAlignedOffset;

    VMemoryView viewToCommit = {
        static_cast<uchar*>( _reservation.ptr ) + pageAlignedOffset,
        commitSize };

    return VMemoryManager::virtualCommit( viewToCommit );
}
void VMemoryArena::decommitRange( uint offset, uint size ) {
    if ( size == 0 || offset + size > _reservation.size )
        return;

    uint pageAlignedOffset = VMemoryManager::alignToPageSize( offset );

    uint endOffset      = offset + size;
    uint pageAlignedEnd = ( endOffset / _pageSize ) * _pageSize;

    if ( pageAlignedEnd <= pageAlignedOffset )
        return;

    uint decommitSize = pageAlignedEnd - pageAlignedOffset;

    VMemoryView viewToDecommit = {
        static_cast<uchar*>( _reservation.ptr ) + pageAlignedOffset,
        decommitSize };

    VMemoryManager::virtualDecommit( viewToDecommit );
}

} // namespace Memory

AXION_NAMESPACE_END