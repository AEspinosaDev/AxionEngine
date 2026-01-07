
#pragma once
#include "GPUScene.h"
#include <Axion/Core/Render/Rasterizer.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

class Rasterizer : public IRasterizer
{
public:
    bool compileShaders( uint threadCount = 0, const std::string filePath = {} ) override;
    void render( const Scene::Scene& scene ) override;
    void shutdown() override;

private:
    void createPipelines();
    void createResources();

    virtual void updateGPUScene( const Scene::Scene& scene );
    virtual void updateResources();

    GPUScene _gpuScene;
};

} // namespace Core::Render

AXION_NAMESPACE_END