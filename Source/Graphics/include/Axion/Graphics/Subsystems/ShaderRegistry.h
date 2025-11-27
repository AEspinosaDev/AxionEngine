#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Graphics/Handle.h"
#include "Axion/Graphics/ShaderCommon.h"
#include <optional>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Graphics {

/// @brief Interface for managing shader compilation, storage, and retrieval.
/// Handles lifecycle, async compilation, and name-to-handle mapping.
class IShaderRegistry
{
public:
    virtual ~IShaderRegistry() = default;

    // Disable copy/move to ensure unique registry ownership.
    IShaderRegistry( const IShaderRegistry& )            = delete;
    IShaderRegistry& operator=( const IShaderRegistry& ) = delete;
    IShaderRegistry( IShaderRegistry&& )                 = delete;
    IShaderRegistry& operator=( IShaderRegistry&& )      = delete;

    class Builder;

    /// @brief Starts the fluent registration process for a new shader. To register it, call load()
    /// @param name Logical name for the shader (used for lookups).
    virtual Builder shader( const std::string& name ) = 0;

    virtual const ShaderBundle&         getBundle( ShaderHandle handle ) const      = 0;
    virtual std::optional<ShaderHandle> findShader( const std::string& name ) const = 0;
    virtual const ShaderBundle&         compileShader( ShaderHandle handle )        = 0;
    virtual const ShaderBundle&         compileShader( const std::string& name )    = 0;
    virtual void                        compileAllShaders( bool async = false )     = 0;
    virtual uint                        size() const                                = 0;

protected:
    IShaderRegistry() = default;

    /// @brief Internal method to register shader metadata without compiling.
    virtual ShaderHandle registerShader( const ShaderDesc& desc ) = 0;

    friend class Builder;
};

/// @brief Fluent builder helper for configuring and registering shaders.
class IShaderRegistry::Builder
{
public:
    Builder( IShaderRegistry& reg, std::string name )
        : _registry( reg ) {
        _desc.name = std::move( name );
    }

    /// @brief Sets the source file path.
    Builder& path( const std::string& p ) {
        _desc.path = p;
        return *this;
    }

    /// @brief Adds an include directory for imports.
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

    /// @brief Sets AutoReflect.
    Builder& autoReflect( bool opt ) {
        _desc.autoReflect = opt;
        return *this;
    }
    // --- STAGES ---
    Builder& vs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Vertex } );
        return *this;
    }
    Builder& ps( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Pixel } );
        return *this;
    }
    Builder& cs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Compute } );
        return *this;
    }
    Builder& gs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Geometry } );
        return *this;
    }
    Builder& hs( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Hull } );
        return *this;
    }
    Builder& ds( const std::string& entryName ) {
        _desc.entryPoints.push_back( { entryName, RHI::ShaderType::Domain } );
        return *this;
    }
    Builder& layout( const RHI::PipelineLayoutDesc& desc ) {
        _desc.layoutDesc = desc;
        _desc.autoReflect  = false;
        return *this;
    }
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