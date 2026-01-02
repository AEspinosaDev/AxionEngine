
#pragma once
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

DEFINE_UNIQUE_PTR_FOR_TYPE( IRenderer, Renderer )

class GPUScene;

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual bool compileShaders( uint threadCount = 0, const std::string filePath = {} ) = 0;
    virtual void render( const Scene::Scene& scene )                                     = 0;
    virtual void shutdown()                                                              = 0;

private:
    virtual void createPipelines() = 0;
    virtual void createResources() = 0;

    // GPU Scene inside will take care of setting GPUMeshes along with materials an all an making necessary
    //  flags enabble for upploading data
    virtual void buildGPUScene( const Scene::Scene& scene, GPUScene& outGPUScene ) = 0;
    virtual void updateResources( const GPUScene& outGPUScene )                    = 0;
};

} // namespace Core::Render

AXION_NAMESPACE_END