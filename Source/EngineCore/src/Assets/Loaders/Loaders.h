#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "MeshUtils.h"
#include <unordered_map>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint>   indices;
};

struct ImageData {
    Extent3D                                                     size;
    uint                                                         channels = 0;
    bool                                                         isHDR    = false;
    std::variant<std::vector<unsigned char>, std::vector<float>> pixels;
    TexturePrecision                                             precision;
};

bool loadOBJ( const std::string& filepath, MeshData& outMesh, MeshImportFlags flags );
// bool loadOBJ( const std::string& filepath, std::unordered_map<Mesh, Material>& assetMap, MeshImportFlags flags );
// bool loadPLY( const std::string& filepath, Mesh& outMesh );
// bool loadGLTF( const std::string& filepath, Mesh& outMesh );

bool loadImage( const std::string& filepath, ImageData& outImage, TextureImportFlags flags );

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END