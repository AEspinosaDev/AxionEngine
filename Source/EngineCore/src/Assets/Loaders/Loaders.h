#include "Axion/Common/Logging.h"
#include "Axion/Core/Assets/AssetManager.h"
#include "MeshUtils.h"
#include <unordered_map>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

struct MeshData {
    STLW::Vector<Vertex> vertices;
    STLW::Vector<u32>    indices;
};

struct ImageData {
    Extent3D size;
    u32      channels = 0;
    bool     isHDR    = false;

    TexturePixels    pixels;
    TexturePrecision precision;
};

bool loadOBJ( const STLW::String& filepath, MeshData& outMesh, MeshImportFlags flags );
// bool loadOBJ( const STLW::String& filepath, std::unordered_map<Mesh, Material>& assetMap, MeshImportFlags flags );
// bool loadPLY( const STLW::String& filepath, Mesh& outMesh );
// bool loadGLTF( const STLW::String& filepath, Mesh& outMesh );

bool loadImage( const STLW::String& filepath, ImageData& outImage, TextureImportFlags flags );

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END