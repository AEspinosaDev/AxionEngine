#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Common/Math.h>
#include <Axion/Core/Assets/Common.h>
#include <Axion/Core/Assets/Handle.h>


AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class AssetManager;

class Material
{
public:
    virtual ~Material() = default;

    AXION_DISABLE_COPY( Material )
    AXION_ENABLE_MOVE( Material )

    [[nodiscard]] const String64& getName() const { return _instanceName; }
    [[nodiscard]] bool            isDirty() const { return _isDirty; }
    void                          clearDirty();

    virtual MaterialArchetypeInfo getArchetypeInfo() const = 0;
    virtual u32                   getPayloadSize() const   = 0;

    virtual STLW::Vector<TextureHandle> getTextureHandles() const = 0;

    using TextureResolver = std::function<u32( const TextureHandle& )>;

    virtual void writePayload( void* dest, const TextureResolver& resolver ) const = 0;

    void setRenderState( const Graphics::RenderState& state ) {
        _rndState = state;
        markDirty();
    }
    const Graphics::RenderState& getRenderState() const { return _rndState; }

protected:
    friend class AssetManager;

    void setOwner( AssetManager* owner, MaterialHandle handle ) {
        _owner  = owner;
        _handle = handle;
    }

    explicit Material( StringView name )
        : _instanceName( name )
        , _isDirty( true ) {}

    void markDirty();

    AssetManager*  _owner = nullptr;
    MaterialHandle _handle;

    String64 _instanceName;
    bool     _isDirty;

    Graphics::RenderState _rndState; // Runtime
};

} // namespace Core::Assets

AXION_NAMESPACE_END
