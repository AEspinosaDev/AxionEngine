#pragma once
#include "Axion/Common/Defines.h"
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
    IShaderRegistry( const IShaderRegistry& )            = delete;
    IShaderRegistry& operator=( const IShaderRegistry& ) = delete;
    IShaderRegistry( IShaderRegistry&& )                 = delete;
    IShaderRegistry& operator=( IShaderRegistry&& )      = delete;

    class Builder;

    /// @brief Starts the fluent registration process for a new shader program.
    /// @param name Unique logical name for the shader (used for lookups).
    virtual Builder shader( const std::string& name ) = 0;

    // -------------------------------------------------------------------------
    // RUNTIME ACCESS
    // -------------------------------------------------------------------------

    /// @brief Retrieves the compiled bundle (bytecode + reflection) for a shader.
    /// @return Reference to the bundle. Returns invalid bundle if compilation failed or handle is bad.
    virtual const ShaderBundle& getBundle( ShaderHandle handle ) const = 0;

    /// @brief Looks up a shader handle by its logical name.
    virtual std::optional<ShaderHandle> findShader( const std::string& name ) const = 0;

    /// @brief Triggers compilation for a specific shader if not already ready.
    /// @return The compiled bundle.
    virtual const ShaderBundle& compileShader( ShaderHandle handle ) = 0;

    /// @brief Triggers compilation by name (Convenience method).
    virtual const ShaderBundle& compileShader( const std::string& name ) = 0;

    /// @brief Compiles all registered shaders that are not yet ready.
    /// @param async If true, compilation happens on worker threads (not implemented yet).
    virtual void compileAllShaders( bool async = false ) = 0;

    /// @brief Returns the total number of registered shaders.
    virtual uint size() const = 0;

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
    Builder( IShaderRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.name = std::move( name );
    }

    /// @brief Sets the source file path (e.g., "Assets/Shaders/MyShader.slang").
    Builder& path( const std::string& p ) {
        _desc.path = p;
        return *this;
    }

    /// @brief Adds an include directory for import resolution.
    Builder& include( const std::string& inc ) {
        _desc.includePaths.push_back( inc );
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
    /// Default is true unless a manual layout is provided.
    Builder& autoReflect( bool opt ) {
        _desc.autoReflect = opt;
        return *this;
    }

    // --- STAGE ENTRY POINTS ---

    /// @brief Adds a Vertex Shader entry point.
    Builder& vs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Vertex } );
        return *this;
    }
    /// @brief Adds a Pixel (Fragment) Shader entry point.
    Builder& ps( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Pixel } );
        return *this;
    }
    /// @brief Adds a Compute Shader entry point.
    Builder& cs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Compute } );
        return *this;
    }
    /// @brief Adds a Geometry Shader entry point.
    Builder& gs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Geometry } );
        return *this;
    }
    /// @brief Adds a Hull (Tessellation Control) Shader entry point.
    Builder& hs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Hull } );
        return *this;
    }
    /// @brief Adds a Domain (Tessellation Evaluation) Shader entry point.
    Builder& ds( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Domain } );
        return *this;
    }
    /// @brief Adds a Raygen Shader entry point.
    Builder& raygen( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::RayGeneration } );
        return *this;
    }
    /// @brief Adds a Miss Shader entry point.
    Builder& miss( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Miss } );
        return *this;
    }
    /// @brief Adds a Closest Hit Shader entry point.
    Builder& closestHit( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::ClosestHit } );
        return *this;
    }
    /// @brief Adds a Callable Shader entry point.
    Builder& callable( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Callable } );
        return *this;
    }

    // --- MANUAL CONFIGURATION ---

    /// @brief Manually defines the Pipeline Layout (Root Signature).
    /// Disables auto-reflection. Useful for fixing layout mismatches or optimization.
    Builder& layout( const RHI::PipelineLayoutDesc& desc ) {
        _desc.layoutDesc  = desc;
        _desc.autoReflect = false;
        return *this;
    }

    /// @brief Manually defines the Vertex Input Attributes.
    /// Overrides auto-reflection for vertex inputs.
    Builder& vertexAttributes( const std::vector<RHI::VertexAttribute>& attrs ) {
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