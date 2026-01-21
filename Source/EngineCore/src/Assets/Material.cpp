#include <Axion/Common/Logging.h>
#include <Axion/Core/Assets/Material.h>
#include <map>

AXION_NAMESPACE_BEGIN
namespace Core::Assets {

static std::map<std::string, GlobalMaterialRegistry::SetupCallback>& getRegistryInternal() {
    static std::map<std::string, GlobalMaterialRegistry::SetupCallback> registry;
    return registry;
}

void GlobalMaterialRegistry::registerMaterial( const std::string& name, SetupCallback callback ) {
    getRegistryInternal()[name] = callback;

    AXION_LOG_INFO( Logger::Module::Core, "[PRE-EXECUTION MSG] REGISTERED MATERIAL CLASS: {}", name );
}

void GlobalMaterialRegistry::enumerate( std::function<void( const std::string& name, SetupCallback callback )> visitor ) {
    auto& reg = getRegistryInternal();
    for ( const auto& [name, callback] : reg )
    {
        visitor( name, callback );
    }
}

} // namespace Core::Assets
AXION_NAMESPACE_END