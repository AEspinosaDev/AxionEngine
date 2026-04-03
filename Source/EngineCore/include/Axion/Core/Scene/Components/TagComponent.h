#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class TagComponent
{
public:
    TagComponent()                      = default;
    TagComponent( const TagComponent& ) = default;
    TagComponent( StringView t )
        : tag( t ) {}

    void       setTag( StringView t ) { tag = t; }
    StringView getTag() const { return tag; }

    operator String64&() { return tag; }
    operator const String64&() const { return tag; }

private:
    String64 tag;
};

} // namespace Core::Scene

AXION_NAMESPACE_END