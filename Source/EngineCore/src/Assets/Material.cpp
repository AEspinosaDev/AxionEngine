#include <Axion/Common/Logging.h>
#include <Axion/Core/Assets/AssetManager.h>
#include <Axion/Core/Assets/Material.h>
#include <map>

AXION_NAMESPACE_BEGIN
namespace Core::Assets {

static std::map<std::string, GlobalMaterialRegistry::SetupCallback>& getRegistryInternal() {
    static std::map<std::string, GlobalMaterialRegistry::SetupCallback> registry;
    return registry;
}

void GlobalMaterialRegistry::registerMaterial( std::string_view name, SetupCallback callback ) 
{
    getRegistryInternal()[std::string(name)] = callback;
    AXION_LOG_INFO( Logger::Module::Core, "[PRE-EXECUTION MSG] REGISTERED MATERIAL CLASS: {}", name );
}

void GlobalMaterialRegistry::enumerate( std::function<void( const std::string& name, SetupCallback callback )> visitor ) {
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