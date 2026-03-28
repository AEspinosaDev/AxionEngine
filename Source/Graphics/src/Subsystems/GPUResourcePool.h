#pragma once
#include "RendererSubsystem.h"

AXION_NAMESPACE_BEGIN
namespace Graphics {

class GPUResourcePool final : public IGPUResourcePool, public RendererSubsystem
{
public:
    explicit GPUResourcePool();
    ~GPUResourcePool() override;

    void initialize( const SubsystemInitContext& ctx ) override;

    BufferBuilder  buffer( const std::string& name ) override { return BufferBuilder( *this, name ); }
    TextureBuilder texture( const std::string& name ) override { return TextureBuilder( *this, name ); }
    SamplerBuilder sampler( const std::string& name ) override { return SamplerBuilder( *this, name ); }
    AccelBuilder   accel( const std::string& name ) override { return AccelBuilder( *this, name ); }

    RHI::IBuffer*                getBuffer( BufferHandle handle ) override;
    std::optional<BufferHandle>  findBuffer( const std::string& name ) const override;
    void                         destroyBuffer( BufferHandle handle ) override;
    RHI::ITexture*               getTexture( TextureHandle handle ) override;
    std::optional<TextureHandle> findTexture( const std::string& name ) const override;
    void                         destroyTexture( TextureHandle handle ) override;
    RHI::ISampler*               getSampler( SamplerHandle handle ) override;
    std::optional<SamplerHandle> findSampler( const std::string& name ) const override;
    void                         destroySampler( SamplerHandle handle ) override;
    RHI::IAccel*                 getAccel( AccelHandle handle ) override;
    std::optional<AccelHandle>   findAccel( const std::string& name ) const override;
    void                         destroyAccel( AccelHandle handle ) override;

    // Special functions for renderer interop
    TextureHandle registerExternalTexture( RHI::TextureOwnerPtr&& ptr, const std::string& name );
    BufferHandle  registerExternalBuffer( RHI::BufferOwnerPtr&& ptr, const std::string& name );
    SamplerHandle registerExternalSampler( RHI::SamplerOwnerPtr&& ptr, const std::string& name );
    AccelHandle   registerExternalAccel( RHI::AccelOwnerPtr&& ptr, const std::string& name );

    void clear() override;
    uint bufferCount() const override { return (uint)_buffers.size(); };
    uint textureCount() const override { return (uint)_textures.size(); };
    uint samplerCount() const override { return (uint)_samplers.size(); };
    uint accelCount() const override { return (uint)_accels.size(); };

private:
    BufferHandle  createBuffer( const RHI::BufferDesc& desc, const void* initialData, bool allowLookup = true ) override;
    TextureHandle createTexture( const RHI::TextureDesc& desc, const void* initialData, bool allowLookup = true ) override;
    SamplerHandle createSampler( const RHI::SamplerDesc& desc, bool allowLookup = true ) override;
    AccelHandle   createAccel( const RHI::AccelDesc& desc, bool instantBuild = false, bool allowLookup = true ) override;

private:
    template <typename T>
    struct ResourceRecord {
        T           ptr = nullptr;
        std::string name;
        ushort      generation = 0;
        bool        alive      = false;
    };

    // Buffers
    std::vector<ResourceRecord<RHI::BufferOwnerPtr>>   _buffers;
    std::unordered_map<std::string, BufferHandle> _buffNameToHandle;
    // Textures
    std::vector<ResourceRecord<RHI::TextureOwnerPtr>>   _textures;
    std::unordered_map<std::string, TextureHandle> _texNameToHandle;
    // Samplers
    std::vector<ResourceRecord<RHI::SamplerOwnerPtr>>   _samplers;
    std::unordered_map<std::string, SamplerHandle> _samplerNameToHandle;
    // Acceleration Structures
    std::vector<ResourceRecord<RHI::AccelOwnerPtr>>   _accels;
    std::unordered_map<std::string, AccelHandle> _accelNameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END
