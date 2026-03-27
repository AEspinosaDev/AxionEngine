#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/RHI/IPipeline.h"
#include <map>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Graphics {

namespace Shader {

/// @brief Target binary format for shader compilation.
enum NativeFormat : uchar
{
    DXIL,   ///< DirectX Intermediate Language (DirectX 12).
    SPIR_V, ///< Standard Portable Intermediate Representation (Vulkan).
    GLSL    ///< OpenGL Shading Language.
};

struct ProgramBundle {
    struct StageBlob {
        ShaderType         type;
        std::vector<uchar> code;
        std::string        entryPointName;
    };

    std::vector<StageBlob>            stageBlobs;
    RHI::PipelineLayoutDesc           layoutDesc;
    std::vector<RHI::VertexAttribute> vertexAttributes;
    bool                              isValid() const { return !stageBlobs.empty(); }
};

/// @brief Configuration descriptor for a shader source.
struct Description {
    std::string                                      path;                ///< Path to the .slang source file.
    std::vector<std::string>                         includePaths;        ///< Additional directories for import resolution.
    NativeFormat                                     format = DXIL;       ///< Target binary format.
    std::vector<EntryPoint>                          entryPoints;         ///< Name of the entry point functions (e.g., "vsMain").
    std::optional<std::vector<PreprocessorDefine>>   preprocessorDefines; ///< Optional preprocessor definitions.
    std::optional<RHI::PipelineLayoutDesc>           layoutDesc;
    std::optional<std::vector<RHI::VertexAttribute>> vertexAttributes;
    bool                                             autoReflect = true; ///< Whether to generate reflection data.
    std::string                                      name;
};

} // namespace Shader

typedef Shader::Description   ShaderDesc;
typedef Shader::ProgramBundle ShaderBundle;

AXION_NAMESPACE_END
} // namespace Graphics
