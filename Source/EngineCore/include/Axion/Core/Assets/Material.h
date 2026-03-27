#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Common/Math.h>
#include <Axion/Core/Assets/Handle.h>
#include <Axion/Core/Render/Common.h>
#include <functional>
#include <string_view>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class AssetManager;

class GlobalMaterialRegistry
{
public:
    using SetupCallback = std::function<void( Render::MaterialArchetypeDesc& )>;

    static void registerMaterial( std::string_view name, SetupCallback callback );

    static void enumerate( std::function<void( const std::string& name, SetupCallback callback )> visitor );
};

class Material
{
public:
    virtual ~Material() = default;

    Material( const Material& )            = delete;
    Material& operator=( const Material& ) = delete;
    Material( Material&& ) noexcept        = default;
    Material& operator=( Material&& )      = default;

    [[nodiscard]] const std::string& getName() const { return _name; }
    [[nodiscard]] bool               isDirty() const { return _isDirty; }
    void                             clearDirty();

    virtual std::string_view           getArchetypeName() const  = 0;
    virtual uint                       getPayloadSize() const    = 0;
    virtual std::vector<TextureHandle> getTextureHandles() const = 0;

    using TextureResolver = std::function<uint( const TextureHandle& )>;

    virtual void writePayload( void* dest, const TextureResolver& resolver ) const = 0;

protected:
    friend class AssetManager;

    void setOwner( AssetManager* owner, MaterialHandle handle ) {
        _owner  = owner;
        _handle = handle;
    }

    explicit Material( std::string name )
        : _name( std::move( name ) )
        , _isDirty( true ) {}

    void markDirty();

    AssetManager*  _owner = nullptr;
    MaterialHandle _handle;

    std::string _name;
    bool        _isDirty;
};

} // namespace Core::Assets

AXION_NAMESPACE_END

#define AXION_DECLARE_MATERIAL_ARCH_NAME( STRING_NAME )        \
public:                                                        \
    static constexpr std::string_view ARCHETYPE = STRING_NAME; \
                                                               \
    virtual std::string_view getArchetypeName() const override { return ARCHETYPE; }

#define AXION_REGISTER_MATERIAL( CLASS_NAME )                                          \
    static void setup##CLASS_NAME( Axion::Core::Render::MaterialArchetypeDesc& desc ); \
                                                                                       \
    namespace {                                                                        \
    struct Register##CLASS_NAME {                                                      \
        Register##CLASS_NAME() {                                                       \
            Axion::Core::Assets::GlobalMaterialRegistry::registerMaterial(             \
                Axion::Core::Assets::CLASS_NAME::ARCHETYPE,                            \
                setup##CLASS_NAME );                                                   \
        }                                                                              \
    };                                                                                 \
    static Register##CLASS_NAME global_reg_##CLASS_NAME;                               \
    }                                                                                  \
                                                                                       \
    static void setup##CLASS_NAME( Axion::Core::Render::MaterialArchetypeDesc& desc )