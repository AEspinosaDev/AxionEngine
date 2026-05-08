#include "ShaderCompiler.h"
#include <filesystem>

AXION_NAMESPACE_BEGIN

namespace Graphics {

void ShaderCompiler::begin() {
    // 1. Create Only One Slang Session for performance
    // CAUTION ! MONO-THREAD
    if ( !_globalSession )
    {
        SlangGlobalSessionDesc desc = {};
        desc.enableGLSL             = true;
        createGlobalSession( &desc, _globalSession.writeRef() );
    }
}

bool ShaderCompiler::compileFile( const ShaderDesc& desc, ShaderBundle& outBundle ) {

    AXION_LOG_ASSERT( _globalSession, Logger::Module::GFX, "No Slang Session" );

    SessionDesc sessionDesc = {};
    TargetDesc  targetDesc  = {};

    switch ( desc.format )
    {
        case Shader::NativeFormat::DXIL:
            targetDesc.format  = SLANG_DXIL;
            targetDesc.profile = _globalSession->findProfile( "sm_6_5" );
            break;
        case Shader::NativeFormat::SPIR_V:
            targetDesc.format  = SLANG_SPIRV;
            targetDesc.profile = _globalSession->findProfile( "glsl_450" );
            break;
        case Shader::NativeFormat::GLSL:
            targetDesc.format  = SLANG_GLSL;
            targetDesc.profile = _globalSession->findProfile( "glsl_450" );
            break;
        default:
            break;
    }
    // --- Fill Slang import/include paths ---
    std::filesystem::path shaderFilePath( static_cast<std::string>( desc.path ) );
    if ( !std::filesystem::exists( shaderFilePath ) )
    {
        AXION_LOG_ERROR( Logger::Module::Shader, "Shader file not found: {}", desc.path );
        return false;
    }

    std::string shaderDir  = shaderFilePath.parent_path().string();
    std::string moduleName = shaderFilePath.stem().string();

    STLW::Vector<const char*> includePtrs;
    includePtrs.reserve( desc.includePaths.size() + 1 );

    includePtrs.push_back( shaderDir.c_str() );

    for ( auto& p : desc.includePaths )
        includePtrs.push_back( p.c_str() );

    sessionDesc.searchPaths     = includePtrs.data();
    sessionDesc.searchPathCount = (SlangInt)includePtrs.size();
    sessionDesc.targets         = &targetDesc;
    sessionDesc.targetCount     = 1;

    // ShaderCompiler.cpp

    STLW::Vector<slang::PreprocessorMacroDesc> macros;

    if ( desc.format == Shader::NativeFormat::DXIL )
    {
        macros.push_back( { "__D3D12__", "1" } );
    } else
    {
        macros.push_back( { "__VULKAN__", "1" } );
        macros.push_back( { "__SPIRV__", "1" } );
    }

    auto preprocessorDefines = desc.preprocessorDefines.value_or( STLW::Vector<Shader::PreprocessorDefine>() );
    for ( auto& macro : preprocessorDefines )
    {
        macros.push_back( { macro.name.cstr(), macro.value.cstr() } );
    }

    sessionDesc.preprocessorMacros     = macros.data();
    sessionDesc.preprocessorMacroCount = (SlangInt)macros.size();

    Slang::ComPtr<ISession> session;
    _globalSession->createSession( sessionDesc, session.writeRef() );

    Slang::ComPtr<IBlob>   diagnostics;
    Slang::ComPtr<IModule> module( session->loadModule( moduleName.c_str(), diagnostics.writeRef() ) );

    if ( diagnostics )
    {
        const char* diagText = (const char*)diagnostics->getBufferPointer();
        AXION_LOG_ERROR( Logger::Module::Shader, "{}", diagText );
    }
    if ( !module )
    {
        AXION_LOG_ERROR( Logger::Module::Shader, "Slang failed to load module {}", desc.path.c_str() );
        return false;
    }

    STLW::Vector<Slang::ComPtr<slang::IEntryPoint>>    entryPointsKeepAlive;
    STLW::Vector<Slang::ComPtr<slang::IModule>>        extraModulesKeepAlive;
    STLW::Vector<Slang::ComPtr<slang::IComponentType>> componentsKeepAlive;
    STLW::Vector<slang::IComponentType*>               rawComponents;

    rawComponents.reserve( desc.entryPoints.size() + 1 + desc.additionalModules.size() );
    rawComponents.push_back( module.get() );

    // Load additional modules
    // 2. Load and add all additional modules (e.g., Material implementations)
    for ( const auto& extraModName : desc.additionalModules )
    {
        Slang::ComPtr<slang::IBlob>   extraDiagnostics;
        Slang::ComPtr<slang::IModule> extraModule( session->loadModule( extraModName.cstr(), extraDiagnostics.writeRef() ) );

        if ( extraDiagnostics )
        {
            const char* diagText = (const char*)extraDiagnostics->getBufferPointer();
            AXION_LOG_WARN( Logger::Module::Shader, "Diagnostics for module {}: {}", extraModName, diagText );
        }

        if ( !extraModule )
        {
            AXION_LOG_ERROR( Logger::Module::Shader, "Slang failed to load additional module '{}'", extraModName );
            return false;
        }

        extraModulesKeepAlive.push_back( extraModule );
        rawComponents.push_back( extraModule.get() );
    }

    if ( desc.entryPoints.empty() )
    {
        AXION_LOG_ERROR( Logger::Module::Shader, "No entry points specified for shader: {}", desc.path.c_str() );
        return false;
    }

    for ( const auto& epDesc : desc.entryPoints )
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        module->findEntryPointByName( epDesc.name.cstr(), entryPoint.writeRef() );

        if ( !entryPoint )
        {
            AXION_LOG_ERROR( Logger::Module::Shader, "Entry point '{}' not found in {}", epDesc.name, desc.path );
            return false;
        }

        // Look for specialization args
        SmallVector<slang::SpecializationArg, 2> specializationArgs;
        for ( const auto& specType : desc.spececializationTypeNames )
        {
            if ( specType.empty() )
                continue;

            slang::TypeReflection* specReflType = module->getLayout()->findTypeByName( specType.cstr() );
            if ( !specReflType )
            {
                AXION_LOG_ERROR( Logger::Module::Shader, "Failed to find concrete type '{}' in layout for specialization.", specType );
                return false;
            }
            specializationArgs.pushBack( { slang::SpecializationArg::Kind::Type, specReflType } );
        }
        // Specialize the entry point
        Slang::ComPtr<slang::IComponentType> specializedEntryPoint = nullptr;
        if ( !specializationArgs.isEmpty() )
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult                 res = entryPoint->specialize(
                specializationArgs.data(),
                specializationArgs.size(),
                specializedEntryPoint.writeRef(),
                diagnosticsBlob.writeRef() );

            if ( diagnosticsBlob )
            {
                const char* diagText = (const char*)diagnosticsBlob->getBufferPointer();
                AXION_LOG_ERROR( Logger::Module::Shader, "Specialization Diagnostics: {}", diagText );
            }
            SLANG_RETURN_ON_FAIL( res );
        }

        entryPointsKeepAlive.push_back( entryPoint );
        componentsKeepAlive.push_back( specializedEntryPoint );
        rawComponents.push_back( specializedEntryPoint ? specializedEntryPoint.get() : entryPoint.get() );
    }

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult                 result = session->createCompositeComponentType(
            rawComponents.data(),
            (SlangInt)rawComponents.size(),
            composedProgram.writeRef(),
            diagnostics.writeRef() );

        if ( diagnostics )
        {
            const char* diagText = (const char*)diagnostics->getBufferPointer();
            AXION_LOG_ERROR( Logger::Module::Shader, "{}", diagText );
        }
        SLANG_RETURN_ON_FAIL( result );
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult                 result = composedProgram->link( linkedProgram.writeRef(), diagnostics.writeRef() );
        if ( diagnostics )
        {
            const char* diagText = (const char*)diagnostics->getBufferPointer();
            AXION_LOG_ERROR( Logger::Module::Shader, "Link Error: {}", diagText );
        }
        SLANG_RETURN_ON_FAIL( result );
    }

    if ( desc.autoReflect )
    {
        extractReflection( desc.name, linkedProgram, outBundle.layoutDesc );
        extractVertexAttributes( linkedProgram, outBundle.vertexAttributes );
    } else
    {
        if ( desc.layoutDesc.has_value() )
        {
            outBundle.layoutDesc = std::move( desc.layoutDesc.value() );
        }
        if ( desc.vertexAttributes.has_value() )
        {
            outBundle.vertexAttributes = std::move( desc.vertexAttributes.value() );
        }
    }

    outBundle.stageBlobs.reserve( desc.entryPoints.size() );
    for ( size_t i = 0; i < desc.entryPoints.size(); ++i )
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IBlob> kernelBlob;
        SlangResult                 result = linkedProgram->getEntryPointCode(
            (int)i,
            0,
            kernelBlob.writeRef(),
            diagnostics.writeRef() );
        if ( diagnostics )
        {
            const char* diagText = (const char*)diagnostics->getBufferPointer();
            AXION_LOG_ERROR( Logger::Module::Shader, "{}", diagText );
        }
        SLANG_RETURN_ON_FAIL( result );

        STLW::Vector<byte> bytecode( kernelBlob->getBufferSize() );
        std::memcpy( bytecode.data(), kernelBlob->getBufferPointer(), bytecode.size() );

        ShaderType type = desc.entryPoints[i].type;
        outBundle.stageBlobs.push_back( { .type           = type,
                                          .code           = std::move( bytecode ),
                                          .entryPointName = desc.entryPoints[i].name } );
    }

    AXION_LOG_INFO( Logger::Module::Shader, "Compiled Shader [{}] successfully.", desc.name );
    return true;
}

void ShaderCompiler::end() {
    _globalSession = nullptr;
}
SlangStage ShaderCompiler::stageToSlang( RHI::ShaderStage stage ) {
    switch ( stage )
    {
        case RHI::ShaderStage::Vertex:
            return SLANG_STAGE_VERTEX;
        case RHI::ShaderStage::Pixel:
            return SLANG_STAGE_FRAGMENT;
        case RHI::ShaderStage::Compute:
            return SLANG_STAGE_COMPUTE;
        default:
            return SLANG_STAGE_NONE;
    }
}
RHI::DescriptorType ShaderCompiler::slangTypeToRHI( slang::TypeReflection* type ) {
    using namespace slang;

    if ( type->getKind() == TypeReflection::Kind::Array )
    {
        return slangTypeToRHI( type->getElementType() );
    }

    TypeReflection::Kind kind = type->getKind();

    switch ( kind )
    {
        case TypeReflection::Kind::ConstantBuffer:
            return RHI::DescriptorType::UniformBuffer; // cbuffer { ... }

        case TypeReflection::Kind::Resource: {
            SlangResourceShape  shape  = type->getResourceShape();
            SlangResourceAccess access = type->getResourceAccess();

            // 0. Accel
            if ( shape == SLANG_ACCELERATION_STRUCTURE )
            {
                return RHI::DescriptorType::AccelerationStructure;
            }
            // 1. Texturas
            if ( shape & SLANG_RESOURCE_BASE_SHAPE_MASK )
            {
                // Si es escritura (RWTexture) -> StorageImage (UAV)
                if ( access == SLANG_RESOURCE_ACCESS_READ_WRITE || access == SLANG_RESOURCE_ACCESS_WRITE )
                    return RHI::DescriptorType::StorageImage;

                // Si es lectura -> SampledImage (SRV)
                return RHI::DescriptorType::SampledImage;
            }

            // 2. Buffers (Structured, ByteAddress, etc.)
            // En muchos RHIs (Vulkan/DX12), tanto SRV como UAV de buffers se tratan como StorageBuffer
            // en el descriptor, aunque DX12 distingue rangos.
            if ( shape & SLANG_STRUCTURED_BUFFER ||
                 shape & SLANG_BYTE_ADDRESS_BUFFER )
            {
                return RHI::DescriptorType::StorageBuffer;
            }
            return RHI::DescriptorType::SampledImage; // Fallback
        }

        case TypeReflection::Kind::SamplerState:
            return RHI::DescriptorType::Sampler;

        // Nota: ParameterBlock se trataría aquí si decides usarlo en el futuro
        case TypeReflection::Kind::ParameterBlock:
            return RHI::DescriptorType::UniformBuffer; // Ojo: Esto depende de cómo lo implementes

        default:
            return RHI::DescriptorType::UniformBuffer; // Fallback
    }
}

Format ShaderCompiler::slangFormatToRHI( slang::TypeReflection* type ) {
    using namespace slang;
    size_t          elemCount  = 1;
    TypeReflection* scalarType = type;

    if ( type->getKind() == TypeReflection::Kind::Vector )
    {
        elemCount  = type->getElementCount();
        scalarType = type->getElementType();
    }

    if ( scalarType->getScalarType() == TypeReflection::ScalarType::Float32 )
    {
        switch ( elemCount )
        {
            case 1:
                return Format::R32_FLOAT;
            case 2:
                return Format::RG32_FLOAT;
            case 3:
                return Format::RGB32_FLOAT;
            case 4:
                return Format::RGBA32_FLOAT;
        }
    }
    // ... Int32, UInt32 ...

    return Format::UNKNOWN;
}

void ShaderCompiler::reflectParameter(
    slang::VariableLayoutReflection*                      varLayout,
    STLW::Map<u32, STLW::Vector<RHI::DescriptorBinding>>& tempSets ) {
    slang::TypeReflection*      type = varLayout->getType();
    slang::TypeReflection::Kind kind = type->getKind();

    // --- CASO 1: Es un recurso directo (Buffer, Texture, Sampler) ---
    // Reutilizamos la lógica de detección de tipo que te di antes
    // Desentrañamos arrays primero
    slang::TypeReflection* checkType = type;
    while ( checkType->getKind() == slang::TypeReflection::Kind::Array )
    {
        checkType = checkType->getElementType();
    }
    slang::TypeReflection::Kind realKind = checkType->getKind();

    bool isResource =
        ( realKind == slang::TypeReflection::Kind::Resource ) ||
        ( realKind == slang::TypeReflection::Kind::SamplerState ) ||
        ( realKind == slang::TypeReflection::Kind::ConstantBuffer );

    if ( isResource )
    {
        u32 bindingIdx = (u32)varLayout->getBindingIndex();
        u32 spaceIdx   = (u32)varLayout->getBindingSpace();

        // Si Slang dice "sin binding", lo ignoramos (o es un error del shader)
        if ( bindingIdx == -1 )
            return;

        RHI::DescriptorBinding bindingInfo;
        bindingInfo.binding   = bindingIdx;
        bindingInfo.type      = slangTypeToRHI( type ); // Tu función de mapeo
        bindingInfo.stageMask = RHI::ShaderStage::All;  // Asumimos visibilidad total
        bindingInfo.arraySize = (u32)type->getElementCount();
        if ( bindingInfo.arraySize == 0 )
            bindingInfo.arraySize = 1;

        // IMPORTANTE: Evitar duplicados si Slang reporta el mismo recurso en Global y en EntryPoint
        // Simplemente sobrescribimos (o chequeamos si existe)
        auto& bindings = tempSets[spaceIdx];
        bool  exists   = false;
        for ( auto& b : bindings )
        {
            if ( b.binding == bindingInfo.binding )
            {
                exists = true;
                break;
            }
        }
        if ( !exists )
            bindings.push_back( bindingInfo );

        return; // Ya procesamos este nodo, no necesitamos entrar más
    }

    // --- CASO 2: Es un Struct o Constant Buffer Block ---
    // Si Slang ha agrupado cosas (o usamos ParameterBlock), entramos recursivamente.
    if ( kind == slang::TypeReflection::Kind::Struct ||
         kind == slang::TypeReflection::Kind::ParameterBlock )
    {
        // Un ParameterBlock tiene su propio sub-layout de campos
        unsigned fieldCount = type->getFieldCount();
        for ( unsigned i = 0; i < fieldCount; i++ )
        {
            // OJO: type->getFieldByIndex da TypeLayout, pero varLayout->getTypeLayout()->getFieldByIndex...
            // La forma correcta de navegar la JERARQUÍA DE VARIABLES es usar getFieldByIndex del type
            // pero necesitamos el VariableLayout correspondiente (offset/binding relativo).

            // Slang a veces expone los hijos directamente si es un ParameterBlock
            // Si es un struct normal usado como uniform, no tiene bindings dentro.
            // PERO si es un struct usado como ParameterBlock, sí.

            // Simplificación: En tu caso (Global Resources), suelen ser top-level.
            // Si ese 'paramCount == 1' es un struct anónimo, esto lo cazaría.

            // Nota: Navegar sub-campos de variables en Slang puede ser complejo porque
            // depende de si es Offset-based (Uniforms) o Register-based.
        }
    }
}
void ShaderCompiler::extractReflection( StringView name, IComponentType* program, RHI::PipelineLayoutDesc& outDesc ) {

    outDesc.sets.clear();
    outDesc.pushConstant = {};

    slang::ProgramLayout* slangLayout = program->getLayout();

    STLW::Map<u32, SmallVector<RHI::DescriptorBinding>> tempSets;

    u32 paramCount = slangLayout->getParameterCount();

    for ( u32 i = 0; i < paramCount; ++i )
    {
        slang::VariableLayoutReflection* varLayout = slangLayout->getParameterByIndex( i );
        slang::TypeReflection*           type      = varLayout->getType();

        slang::TypeReflection* checkType = type;
        if ( checkType->getKind() == slang::TypeReflection::Kind::Array )
        {
            checkType = checkType->getElementType();
        }

        bool isPushConstant = false;

        // Buscamos el atributo para push constant
        slang::VariableReflection* varReflection = varLayout->getVariable();
        bool                       hasAttribute  = false;

        if ( varReflection )
        {
            if ( varReflection->findUserAttributeByName( _globalSession, "vk::push_constant" ) != nullptr )
            {
                isPushConstant = true;
            }
        }

        StringView name = varLayout->getName();
        if ( name == "PushConstants" || name == "gPush" )
        {
            isPushConstant = true;
        }

        if ( isPushConstant )
        {
            slang::TypeLayoutReflection* typeLayout = varLayout->getTypeLayout();

            // --- FIX: DESENVOLVER EL CBUFFER ---
            // Si el tipo es un ConstantBuffer (el wrapper), queremos el tamaño de lo que hay DENTRO.
            if ( typeLayout->getKind() == slang::TypeReflection::Kind::ConstantBuffer )
            {
                typeLayout = typeLayout->getElementTypeLayout();
            }
            // -----------------------------------

            size_t sizeBytes = typeLayout->getSize();

            // Fallback de seguridad por si acaso sigue siendo 0 (ej: struct vacío)
            if ( sizeBytes == 0 )
            {
                AXION_LOG_WARN( Logger::Module::Shader, "PushConstant '{}' has 0 size. Defaulting to 4.", name );
                sizeBytes = 4;
            }

            // Alinear a 4 bytes (DWORDs)
            if ( sizeBytes % 4 != 0 )
                sizeBytes += ( 4 - ( sizeBytes % 4 ) );

            outDesc.pushConstant.size           = (u32)sizeBytes;
            outDesc.pushConstant.stageMask      = RHI::ShaderStage::All;
            u32 assignedRegister                = varLayout->getBindingIndex();
            u32 assignedSpace                   = varLayout->getBindingSpace();
            outDesc.pushConstant.customRegister = assignedRegister;
            outDesc.pushConstant.customSpace    = assignedSpace;

            AXION_LOG_INFO( Logger::Module::Shader, "Detected Push Constants: {} ({} bytes)", name, sizeBytes );

            continue;
        }

        slang::TypeReflection::Kind typeKind = checkType->getKind();

        bool isDescriptor =
            ( typeKind == slang::TypeReflection::Kind::Resource ) ||
            ( typeKind == slang::TypeReflection::Kind::SamplerState ) ||
            ( typeKind == slang::TypeReflection::Kind::ConstantBuffer ) ||
            ( typeKind == slang::TypeReflection::Kind::ParameterBlock );

        // Si es un float, int, struct normal (no CBuffer), lo ignoramos.
        if ( !isDescriptor )
            continue;

        size_t bindingIdx = varLayout->getBindingIndex();
        size_t spaceIdx   = varLayout->getBindingSpace();

        // Si no tiene binding explícito y Slang no le asignó uno auto, lo saltamos con warning
        // (Aunque para globales suele asignar auto si no usas register)
        if ( bindingIdx == unsigned( -1 ) )
        {
            // Opcional: Log warning "Global resource X has no binding"
            AXION_LOG_WARN( Logger::Module::Shader, "Global resource {} has no binding", name );
            continue;
        }

        RHI::DescriptorBinding bindingInfo;
        bindingInfo.binding   = (u32)bindingIdx;
        bindingInfo.type      = slangTypeToRHI( type );
        bindingInfo.stageMask = RHI::ShaderStage::All;

        bindingInfo.arraySize = (u32)type->getElementCount();
        if ( bindingInfo.arraySize == 0 )
            bindingInfo.arraySize = 1;

        tempSets[(u32)spaceIdx].pushBack( bindingInfo );
    }

    if ( !tempSets.empty() )
    {
        u32 maxSet = tempSets.rbegin()->first;
        outDesc.sets.resize( maxSet + 1 );

        for ( auto& [setIdx, bindings] : tempSets )
        {
            outDesc.sets[setIdx].bindings = bindings;
        }
    }

    outDesc.debugName = StringView( "Autolayout Shader " );
    outDesc.debugName = outDesc.debugName + name;
}
void ShaderCompiler::extractVertexAttributes( slang::IComponentType* program, STLW::Vector<RHI::VertexAttribute>& outAttribs ) {
    outAttribs.clear();
    slang::ProgramLayout* layout = program->getLayout();

    for ( u32 i = 0; i < layout->getEntryPointCount(); ++i )
    {
        slang::EntryPointLayout* ep = layout->getEntryPointByIndex( i );

        if ( ep->getStage() == SLANG_STAGE_VERTEX )
        {
            for ( u32 j = 0; j < ep->getParameterCount(); ++j )
            {
                slang::VariableLayoutReflection* paramVar = ep->getParameterByIndex( j );

                if ( paramVar->getCategory() != slang::ParameterCategory::VaryingInput )
                    continue;

                slang::TypeLayoutReflection* typeLayout = paramVar->getTypeLayout();
                slang::TypeReflection*       type       = typeLayout->getType();

                // CASO A: El input es un Struct (lo normal: struct VSInput { ... })
                if ( type->getKind() == slang::TypeReflection::Kind::Struct )
                {
                    u32 fieldCount = type->getFieldCount();
                    for ( u32 k = 0; k < fieldCount; ++k )
                    {
                        slang::VariableLayoutReflection* fieldVar = typeLayout->getFieldByIndex( k );

                        const char* semanticName = fieldVar->getSemanticName();

                        if ( semanticName )
                        {
                            RHI::VertexAttribute attr;
                            attr.semanticName  = StringView( semanticName );
                            attr.semanticIndex = (u32)fieldVar->getSemanticIndex();

                            // ¡OJO! Slang reporta offsets relativos dentro del struct.
                            // Pero para el InputLayout, queremos el Binding (Slot) del buffer.
                            // Slang suele asignar binding al parámetro padre ('input'), no a los campos.
                            // Asumimos que todo el struct viene del Slot 0 (VBO 0).
                            // Si soportas múltiples VBOs, necesitarías atributos custom o lógica extra.
                            attr.inputSlot = (u32)paramVar->getBindingIndex();

                            // Offset dentro del vértice
                            // getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT)
                            // attr.alignedByteOffset = (uint)fieldVar->getOffset( slang::ParameterCategory::VaryingInput );
                            attr.alignedByteOffset = AUTO_VAL;

                            attr.format = slangFormatToRHI( fieldVar->getType() );

                            outAttribs.push_back( attr );
                        }
                    }
                }
                // CASO B: Inputs sueltos (estilo antiguo: float3 pos : POSITION)
                else
                {
                    const char* semanticName = paramVar->getSemanticName();
                    if ( semanticName )
                    {
                        RHI::VertexAttribute attr;
                        attr.semanticName      = StringView( semanticName );
                        attr.semanticIndex     = (u32)paramVar->getSemanticIndex();
                        attr.inputSlot         = (u32)paramVar->getBindingIndex();
                        attr.alignedByteOffset = 0; // Es el único
                        attr.format            = slangFormatToRHI( type );
                        outAttribs.push_back( attr );
                    }
                }
            }
            break;
        }
    }
}
} // namespace Graphics
// namespace Graphics
AXION_NAMESPACE_END
