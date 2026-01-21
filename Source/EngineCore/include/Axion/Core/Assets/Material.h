#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Graphics/Defines.h>
#include <Axion/Common/Math.h>
#include <Axion/Core/Assets/Handle.h>
#include <Axion/Core/Render/MaterialArchetype.h>
#include <functional>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

class AssetManager;

class GlobalMaterialRegistry
{
public:
    using SetupCallback = std::function<void( Render::MaterialArchetypeDesc& )>;

    static void registerMaterial( const std::string& name, SetupCallback callback );
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
    // Dirty Flag System
    [[nodiscard]] bool isDirty() const { return _isDirty; }
    void               clearDirty() { _isDirty = false; }

    virtual uint getGPUPayloadSize() const = 0;

    using TextureResolver                                                             = std::function<uint32_t( TextureHandle )>;
    virtual void writeGPUPayload( void* dest, const TextureResolver& resolver ) const = 0;

protected:
    friend class AssetManager;

    explicit Material( std::string name )
        : _name( std::move( name ) )
        , _isDirty( true ) {}

    void markDirty() { _isDirty = true; }

    std::string _name;
    bool        _isDirty;
};

} // namespace Core::Assets

AXION_NAMESPACE_END

#define AXION_REGISTER_MATERIAL( CLASS_NAME, STRING_NAME )                                                   \
                                                                                                             \
    static void setup##CLASS_NAME( Axion::Core::Render::MaterialArchetypeDesc& desc );                       \
                                                                                                             \
    namespace {                                                                                              \
    struct Register##CLASS_NAME {                                                                            \
        Register##CLASS_NAME() {                                                                             \
                                                                                                             \
            Axion::Core::Assets::GlobalMaterialRegistry::registerMaterial( STRING_NAME, setup##CLASS_NAME ); \
        }                                                                                                    \
    };                                                                                                       \
    static Register##CLASS_NAME global_reg_##CLASS_NAME;                                                     \
    }                                                                                                        \
                                                                                                             \
    static void setup##CLASS_NAME( Axion::Core::Render::MaterialArchetypeDesc& desc )
