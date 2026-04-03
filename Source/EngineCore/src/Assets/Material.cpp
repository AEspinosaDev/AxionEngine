#include <Axion/Common/Containers/STLWrapper/Maps.h>
#include <Axion/Common/Logging.h>
#include <Axion/Core/Assets/AssetManager.h>
#include <Axion/Core/Assets/Material.h>


AXION_NAMESPACE_BEGIN
namespace Core::Assets {

static STLW::Map<String64, GlobalMaterialRegistry::SetupCallback>& getRegistryInternal() {
    static STLW::Map<String64, GlobalMaterialRegistry::SetupCallback> registry;
    return registry;
}

void GlobalMaterialRegistry::registerMaterial( StringView name, SetupCallback callback ) {
    getRegistryInternal()[String64( name )] = callback;
    AXION_LOG_INFO( Logger::Module::Core, "[PRE-EXECUTION MSG] REGISTERED MATERIAL CLASS: {}", name );
}

void GlobalMaterialRegistry::enumerate( std::function<void( StringView name, SetupCallback callback )> visitor ) {
    auto& reg = getRegistryInternal();
    for ( const auto& [name, callback] : reg )
    {
        visitor( name, callback );
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