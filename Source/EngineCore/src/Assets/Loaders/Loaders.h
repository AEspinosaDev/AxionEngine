#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "MeshUtils.h"
#include <unordered_map>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

bool loadOBJ( const std::string& filepath, Mesh& outMesh, MeshImportFlags flags );
bool loadOBJ( const std::string& filepath, std::unordered_map<Mesh, Material>& assetMap, MeshImportFlags flags );

// bool loadPLY( const std::string& filepath, Mesh& outMesh );
// bool loadGLTF( const std::string& filepath, Mesh& outMesh );

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END