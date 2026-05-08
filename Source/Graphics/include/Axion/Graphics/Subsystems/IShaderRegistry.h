#pragma once
#include "Axion/Common/Common.h"
#include "Axion/Graphics/Handle.h"
#include "Axion/Graphics/ShaderCommon.h"
#include <optional>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Graphics {

/// @brief Interface for the centralized management of Shader Programs.
/// Handles source files, multi-stage entry points, compilation, reflection, and storage.
class IShaderRegistry
{
public:
    virtual ~IShaderRegistry() = default;

    // Non-copyable / Non-movable (Owned by Renderer)
    AXION_DISABLE_COPY_AND_MOVE( IShaderRegistry )

    class Builder;

    /// @brief Starts the fluent registration process for a new shader program.
    /// @param name Unique logical name for the shader (used for lookups).
    virtual Builder shader( StringView name ) = 0;

    // -------------------------------------------------------------------------
    // RUNTIME ACCESS
    // -------------------------------------------------------------------------

    /// @brief Retrieves the compiled bundle (bytecode + reflection) for a shader.
    /// @return Reference to the bundle. Returns invalid bundle if compilation failed or handle is bad.
    virtual const ShaderBundle& getBundle( ShaderHandle handle ) const = 0;

    /// @brief Looks up a shader handle by its logical name.
    virtual std::optional<ShaderHandle> findShader( StringView name ) const = 0;

    /// @brief Triggers compilation for a specific shader if not already ready.
    /// @return The compiled bundle.
    virtual const ShaderBundle& compileShader( ShaderHandle handle ) = 0;

    /// @brief Triggers compilation by name (Convenience method).
    virtual const ShaderBundle& compileShader( StringView name ) = 0;

    /// @brief Compiles all registered shaders that are not yet ready.
    /// @param async If true, compilation happens on worker threads (not implemented yet).
    virtual void compileAllShaders( u32 threadCount = 1 ) = 0;

    /// @brief Returns the total number of registered shaders.
    virtual u32 size() const = 0;

protected:
    IShaderRegistry() = default;

    /// @brief Internal method called by Builder::load() to store metadata.
    virtual ShaderHandle registerShader( const ShaderDesc& desc ) = 0;

    friend class Builder;
};

// -----------------------------------------------------------------------------
// BUILDER
// -----------------------------------------------------------------------------

/// @brief Fluent builder helper for configuring and registering shader programs.
class IShaderRegistry::Builder
{
public:
    Builder( IShaderRegistry& reg, StringView name )
        : _registry( reg ) {
        _desc.name = name;
    }

    /// @brief Sets the source file path (e.g., "Assets/Shaders/MyShader.slang").
    Builder& path( const STLW::String& p ) {
        _desc.path = p;
        return *this;
    }

    /// @brief Adds an include directory for import resolution.
    Builder& include( const STLW::String& p ) {
        _desc.includePaths.push_back( p );
        return *this;
    }

    /// @brief Sets target format to DXIL (DirectX 12).
    Builder& asDXIL() {
        _desc.format = Shader::DXIL;
        return *this;
    }

    /// @brief Sets target format to SPIR-V (Vulkan).
    Builder& asSPIRV() {
        _desc.format = Shader::SPIR_V;
        return *this;
    }

    /// @brief Enables or disables automatic reflection via Slang.
    /// Default is true
    Builder& autoReflect( bool opt ) {
        _desc.autoReflect = opt;
        return *this;
    }

    /// @brief Adds a preprocessor define.
    Builder& addDefine( Shader::PreprocessorDefine define ) {
        if ( !_desc.preprocessorDefines.has_value() )
            _desc.preprocessorDefines.emplace();

        _desc.preprocessorDefines->push_back( std::move( define ) );

        return *this;
    }
    /// @brief Adds a module.
    Builder& addModule( StringView moduleName ) {
        _desc.additionalModules.push_back( moduleName );
        return *this;
    }
    /// @brief Adds a specialization type.
    Builder& addSpecialization( StringView typeName ) {
        _desc.spececializationTypeNames.push_back( typeName );
        return *this;
    }

    // --- STAGE ENTRY POINTS ---

    /// @brief Adds a Vertex Shader entry point.
    Builder& vs( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Vertex } );
        return *this;
    }
    /// @brief Adds a Pixel (Fragment) Shader entry point.
    Builder& ps( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Pixel } );
        return *this;
    }
    /// @brief Adds a Compute Shader entry point.
    Builder& cs( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Compute } );
        return *this;
    }
    /// @brief Adds a Geometry Shader entry point.
    Builder& gs( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Geometry } );
        return *this;
    }
    /// @brief Adds a Hull (Tessellation Control) Shader entry point.
    Builder& hs( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Hull } );
        return *this;
    }
    /// @brief Adds a Domain (Tessellation Evaluation) Shader entry point.
    Builder& ds( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Domain } );
        return *this;
    }
    /// @brief Adds a Raygen Shader entry point.
    Builder& raygen( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::RayGeneration } );
        return *this;
    }
    /// @brief Adds a Miss Shader entry point.
    Builder& miss( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Miss } );
        return *this;
    }
    /// @brief Adds a Closest Hit Shader entry point.
    Builder& closestHit( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::ClosestHit } );
        return *this;
    }
    /// @brief Adds a Callable Shader entry point.
    Builder& callable( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Callable } );
        return *this;
    }
    /// @brief Adds a Mesh Shader entry point.
    Builder& ms( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Mesh } );
        return *this;
    }
    /// @brief Adds an Amplification Shader entry point.
    Builder& as( StringView entryName ) {
        _desc.entryPoints.push_back( { entryName, ShaderType::Amplification } );
        return *this;
    }

    // --- MANUAL CONFIGURATION ---

    /// @brief Manually defines the entire shader entry points description.
    Builder& entryPoints( const STLW::Vector<Shader::EntryPoint>& ep ) {
        _desc.entryPoints = ep;
        return *this;
    }

    /// @brief Manually defines the Pipeline Layout (Root Signature).
    /// Disables auto-reflection. Useful for fixing layout mismatches or optimization.
    Builder& layout( const RHI::PipelineLayoutDesc& desc ) {
        _desc.layoutDesc  = desc;
        _desc.autoReflect = false;
        return *this;
    }

    /// @brief Manually defines the Vertex Input Attributes.
    /// Overrides auto-reflection for vertex inputs.
    Builder& vertexAttributes( const STLW::Vector<RHI::VertexAttribute>& attrs ) {
        _desc.vertexAttributes = attrs;
        return *this;
    }

    /// @brief Finalizes configuration and registers the shader.
    /// @return The handle to the registered shader.
    ShaderHandle load() {
        return _registry.registerShader( _desc );
    }

private:
    IShaderRegistry& _registry;
    ShaderDesc       _desc;
};

} // namespace Graphics
AXION_NAMESPACE_END