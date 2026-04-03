#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

namespace Core::ECS {

using EntityID = u32;

// Invalid/null entity
static const EntityID NULL_ENTITY = 0xFFFFFFFF;

// Maximum number of entities allowed (optional, used for resizing sparse arrays)
static const EntityID MAX_ENTITIES = 100000;

} // namespace Core::ECS

AXION_NAMESPACE_END