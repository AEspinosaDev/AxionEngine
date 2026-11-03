#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/RHI/IPipeline.h"
#include <map>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Graphics {

namespace Shader {

/// @brief Target binary format for shader compilation.
enum NativeFormat : byte
{
    DXIL,   ///< DirectX Intermediate Language (DirectX 12).
    SPIR_V, ///< Standard Portable Intermediate Representation (Vulkan).
    GLSL    ///< OpenGL Shading Language.
};

struct ProgramBundle {
    struct StageBlob {
        ShaderType         type;
        STLW::Vector<byte> code;
        String32           entryPointName;
    };

    STLW::Vector<StageBlob>            stageBlobs;
    RHI::PipelineLayoutDesc            layoutDesc;
    STLW::Vector<RHI::VertexAttribute> vertexAttributes;
    bool                               isValid() const { return !stageBlobs.empty(); }
};

/// @brief Configuration descriptor for a shader source.
struct Description {
    // Main config
    STLW::String               path;          ///< Path to the .slang source file.
    STLW::Vector<STLW::String> includePaths;  ///< Additional directories for import resolution.
    NativeFormat               format = DXIL; ///< Target binary format.
    STLW::Vector<EntryPoint>   entryPoints;   ///< Name of the entry point functions (e.g., "vsMain").
    // Specialization
    STLW::Vector<String64>                          additionalModules;         ///< Additional modules to include.
    STLW::Vector<String64>                          spececializationTypeNames; ///< Name of the specialization type for this shader,
    std::optional<STLW::Vector<PreprocessorDefine>> preprocessorDefines;       ///< Optional preprocessor definitions.
    // Reflection
    std::optional<RHI::PipelineLayoutDesc>            layoutDesc;
    std::optional<STLW::Vector<RHI::VertexAttribute>> vertexAttributes;
    bool                                              autoReflect = true; ///< Whether to generate reflection data.

    String64 name;
};

} // namespace Shader

/// @brief Register map of the DECLARE_* pass I/O macros (Shaders/Slang/Common/Defines.slang). Space 1 of the global layout.
namespace PassIO {
constexpr u32 SET_INDEX  = 1;
constexpr u32 SLOT_COUNT = 8;
constexpr u32 texture( u32 slot ) { return slot; }               // DECLARE_TEXTURE    -> t0 - t7
constexpr u32 buffer( u32 slot ) { return SLOT_COUNT + slot; }   // DECLARE_BUFFER     -> t8 - t15
constexpr u32 rwTexture( u32 slot ) { return slot; }             // DECLARE_RW_TEXTURE -> u0 - u7
constexpr u32 rwBuffer( u32 slot ) { return SLOT_COUNT + slot; } // DECLARE_RW_BUFFER  -> u8 - u15
} // namespace PassIO

typedef Shader::Description   ShaderDesc;
typedef Shader::ProgramBundle ShaderBundle;

AXION_NAMESPACE_END
} // namespace Graphics
