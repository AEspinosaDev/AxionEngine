#pragma once
#include "Axion/Graphics/Renderer.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::Passes {

/// @brief Base interface for render passes.
/// Allows storing different passes in a container (e.g., for initialization).
struct IPass {
    virtual ~IPassBase() = default;

    /// @brief Called once at engine startup to create pipelines and shaders.
    virtual void init( IRenderer& rnd ) = 0;
};

/// @brief Templated interface to enforce the RenderGraph signature.
/// @tparam T The data structure used to share handles between Setup and Execute.
template <typename T>
struct IPassRecipe : public IPass {
    using Data = T; //C++ Hack

    virtual void setup( RenderPassBuilder& builder, T& data )     = 0;
    virtual void execute( const T& data, RenderPassContext& ctx ) = 0;
};

} // namespace Graphics::Passes

AXION_NAMESPACE_END
