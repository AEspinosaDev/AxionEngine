#pragma once
#include <Axion/Graphics/Subsystems/IRenderGraph.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Render graph handles of the persistent global buffers (space 0).
/// Any pass binding the persistent set must declare ALL of them: static descriptors (DATA_STATIC_WHILE_SET_AT_EXECUTE)
/// validate the state of every resource in the table at bind time, even the ones the shader doesn't access
struct GlobalBufferHandles {
    Graphics::RGResourceHandle vertex;
    Graphics::RGResourceHandle index;
    Graphics::RGResourceHandle meshlets;
    Graphics::RGResourceHandle meshletVertices;
    Graphics::RGResourceHandle meshletPrimitives;
    Graphics::RGResourceHandle materials;

    /// @brief Declares every global buffer as shader read (passes binding the persistent set)
    void read( Graphics::RenderPassBuilder& pb ) { declare( pb, Graphics::RHI::ResourceState::ShaderResource, false ); }
    /// @brief Declares every global buffer as copy destination (upload)
    void write( Graphics::RenderPassBuilder& pb ) { declare( pb, Graphics::RHI::ResourceState::CopyDest, true ); }

private:
    void declare( Graphics::RenderPassBuilder& pb, Graphics::RHI::ResourceState state, bool isWrite ) {
        for ( auto* h : { &vertex, &index, &meshlets, &meshletVertices, &meshletPrimitives, &materials } )
            *h = isWrite ? pb.write( *h, state ) : pb.read( *h, state );
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END
