#pragma once
#include "Axion/Common/Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

/**
 * @brief Numeric Handle for GPU objects and resources
 *
 * @tparam T
 */
template <typename T>
struct Handle {
    u32 id = AXION_INVALID_U32;
    bool     isValid() const { return id != AXION_INVALID_U32; }

    bool operator==( const Handle& other ) {
        return id == other.id;
    }
    bool operator!=( const Handle& other ) {
        return id != other.id;
    }
   
};

struct BufferTag {
};
struct TextureTag {
};
struct SamplerTag {
};
struct AccelTag {
};
struct ShaderTag {
};
struct PipelineTag {
};
struct PipelineLayoutTag {
};

using BufferHandle  = Handle<struct BufferTag>;
using TextureHandle = Handle<struct TextureTag>;
using SamplerHandle = Handle<struct SamplerTag>;
using AccelHandle   = Handle<struct AccelTag>;

using ShaderHandle = Handle<struct ShaderTag>;

using PipelineHandle       = Handle<struct PipelineTag>;
using PipelineLayoutHandle = Handle<struct PipelineLayoutTag>;

} // namespace Graphics

AXION_NAMESPACE_END
