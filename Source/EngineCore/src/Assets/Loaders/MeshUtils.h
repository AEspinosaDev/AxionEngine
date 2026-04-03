#include "Axion/Common/Math.h"
#include "Axion/Core/Assets/Mesh.h"
#include <meshoptimizer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

void computeTangents( std::vector<Vertex>& vertices, const std::vector<u32>& indices );
MeshletData cookMeshlets( const std::vector<Vertex>& vertices, const std::vector<u32>& indices );

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END