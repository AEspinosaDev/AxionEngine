#pragma once
#include "Axion/Graphics/Subsystems/IPipelineRegistry.h"
#include "Axion/Graphics/Subsystems/IShaderRegistry.h"
#include "typeindex"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

DEFINE_OWNER_PTR_FOR_TYPE( IRenderPass, RenderPass )

class IRenderPass
{
public:
    virtual ~IRenderPass() = default;

    virtual void registerShaders( Graphics::IShaderRegistry& shaders )     = 0;
    virtual void createPipelines( Graphics::IPipelineRegistry& pipelines ) = 0;
};

class PassManager
{
public:
    template <typename T>
    void registerPass() {
        // auto pass                = std::make_unique<T>();
        auto pass                = Memory::makeOwned<T>();
        _passLookup[typeid( T )] = pass.get();

        _passes.push_back( std::move( pass ) );
    }

    template <typename T>
    T* getPass() {
        auto it = _passLookup.find( typeid( T ) );
        if ( it != _passLookup.end() )
        {
            return static_cast<T*>( it->second );
        }
        return nullptr;
    }

    void registerShaders( Graphics::IShaderRegistry& shaders ) {
        for ( auto& pass : _passes )
            pass->registerShaders( shaders );
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) {
        for ( auto& pass : _passes )
            pass->createPipelines( pipelines );
    }

private:
    STLW::Vector<RenderPassOwnerPtr>                  _passes;
    STLW::UnorderedMap<std::type_index, IRenderPass*> _passLookup;
};

} // namespace Core::Render
AXION_NAMESPACE_END