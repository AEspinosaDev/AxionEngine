#pragma once
#include <Axion/Common/Defines.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct TagComponent {
    std::string tag;

    TagComponent()                      = default;
    TagComponent( const TagComponent& ) = default;
    TagComponent( const std::string& t )
        : tag( t ) {}

    operator std::string&() { return tag; }
    operator const std::string&() const { return tag; }
};

} // namespace Core::Scene

AXION_NAMESPACE_END