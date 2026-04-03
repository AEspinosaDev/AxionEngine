#pragma once
#include <Axion/Graphics/RHI/Common.h>
#include <Axion/Graphics/Subsystems/IShaderRegistry.h>
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

AXION_NAMESPACE_BEGIN

namespace Graphics {

using namespace slang;

class ShaderCompiler
{
public:
    void begin();
    bool compileFile( const ShaderDesc& desc, ShaderBundle& outBundle );
    void end();

private:
    SlangStage          stageToSlang( RHI::ShaderStage stage );
    RHI::DescriptorType slangTypeToRHI( slang::TypeReflection* type );
    Format              slangFormatToRHI( slang::TypeReflection* type );

    void reflectParameter( slang::VariableLayoutReflection* varLayout, STLW::Map<uint32_t, STLW::Vector<RHI::DescriptorBinding>>& tempSets );
    void extractReflection( StringView name, slang::IComponentType* program, RHI::PipelineLayoutDesc& outDesc );
    void extractVertexAttributes( slang::IComponentType* program, STLW::Vector<RHI::VertexAttribute>& outAttribs );

    Slang::ComPtr<IGlobalSession> _globalSession = nullptr;

}; // namespace ShaderCompiler

} // namespace Graphics
AXION_NAMESPACE_END