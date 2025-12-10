#pragma once
#include "Axion/Common/Defines.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

/**
 * @brief Numeric Handle for GPU objects and resources
 *
 * @tparam T
 */
template <typename T>
struct Handle {
    uint32_t id = UINT32_MAX;
    bool     isValid() const { return id != UINT32_MAX; }
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

using BufferHandle  = Handle<struct BufferTag>;
using TextureHandle = Handle<struct TextureTag>;
using SamplerHandle = Handle<struct SamplerTag>;
using AccelHandle   = Handle<struct AccelTag>;

using ShaderHandle = Handle<struct ShaderTag>;

using PipelineHandle = Handle<struct PipelineTag>;

} // namespace Graphics

AXION_NAMESPACE_END
