#include <Axion/Common/Containers/STLWrapper/Maps.h>
#include <Axion/Common/Logging.h>
#include <Axion/Core/Assets/AssetManager.h>
#include <Axion/Core/Assets/Material.h>


AXION_NAMESPACE_BEGIN
namespace Core::Assets {

static STLW::Map<String64, MaterialArchetypeInfo>& getRegistryInternal() {
    static STLW::Map<String64, MaterialArchetypeInfo> registry;
    return registry;
}

void GlobalMaterialRegistry::registerMaterial( StringView name, MaterialArchetypeInfo info ) {
    getRegistryInternal()[String64( name )] = info;
    AXION_LOG_INFO( Logger::Module::Core, "[PRE-EXECUTION MSG] REGISTERED MATERIAL CLASS: {}", name );
}

void GlobalMaterialRegistry::enumerate( std::function<void( StringView name, MaterialArchetypeInfo info )> visitor ) {
    auto& reg = getRegistryInternal();
    for ( const auto& [name, info] : reg )
    {
        visitor( name, info );
    }
}

void Material::clearDirty() {
    if ( _isDirty )
    {
        _isDirty = false;
        if ( _owner )
        {
            _owner->notifyMaterialDirty( _handle, false );
        }
    }
}

void Material::markDirty() {
    if ( !_isDirty )
    {
        _isDirty = true;
        if ( _owner )
        {
            _owner->notifyMaterialDirty( _handle, true );
        }
    }
}

} // namespace Core::Assets
AXION_NAMESPACE_END