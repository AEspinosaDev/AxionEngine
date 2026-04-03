#include "Axion/Common/Math.h"
#include "Axion/Core/Assets/Mesh.h"
#include <meshoptimizer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

void        computeTangents( STLW::Vector<Vertex>& vertices, const STLW::Vector<u32>& indices );
MeshletData cookMeshlets( const STLW::Vector<Vertex>& vertices, const STLW::Vector<u32>& indices );

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END