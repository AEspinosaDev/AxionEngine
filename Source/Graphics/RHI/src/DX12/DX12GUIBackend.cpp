#pragma once
#include "DX12GUIBackend.h"
#include "DX12Device.h"
#include "DX12TranslatorUnit.h"
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_win32.h>


AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {
GUIBackendOwnerPtr RHI::createGUIBackendForDX12( IDevice* device, const GUIBackendDesc& desc ) {
    return Memory::makeOwned<DX12GUIBackend>( static_cast<DX12Device*>( device )->_ctx, desc );
}

DX12GUIBackend::DX12GUIBackend( DX12Device::Context& ctx, const GUIBackendDesc& desc )
    : _gpuHeapSRV( ctx.gpuHeapSRV )
    , _desc( desc ) {

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.ConfigFlags |= desc.configFlags;

    _fontSlot = _gpuHeapSRV.allocate( 1 );

    // Initialize platform backend
    if ( _desc.platform == PlatformType ::Win32 )
    {
        ImGui_ImplWin32_Init( _desc.nativeWindowHandle );
        _platformNewFrame = ImGui_ImplWin32_NewFrame;
        _platformShutdown = ImGui_ImplWin32_Shutdown;
    } else if ( _desc.platform == PlatformType::GLFW )
    {
        ImGui_ImplGlfw_InitForOther( static_cast<GLFWwindow*>( _desc.nativeWindowHandle ), false ); // We handle callbacks ourselves, so no need for ImGui to install them
        _platformNewFrame = ImGui_ImplGlfw_NewFrame;
        _platformShutdown = ImGui_ImplGlfw_Shutdown;
    }

    ImGui_ImplDX12_InitInfo init_info      = {};
    init_info.Device                       = ctx.device.Get();
    init_info.CommandQueue                 = ctx.primaryQueue->queue.Get();
    init_info.NumFramesInFlight            = _desc.framesInFlight;
    init_info.RTVFormat                    = DX12Translator::get( _desc.backbufferFormat );
    init_info.DSVFormat                    = DXGI_FORMAT_UNKNOWN; // Assuming no depth buffer for UI
    init_info.SrvDescriptorHeap            = _gpuHeapSRV.getHeap();
    init_info.LegacySingleSrvCpuDescriptor = _gpuHeapSRV.getCPU( (u32)_fontSlot.offset );
    init_info.LegacySingleSrvGpuDescriptor = _gpuHeapSRV.getGPU( (u32)_fontSlot.offset );

    // Initialize using the new API
    if ( ImGui_ImplDX12_Init( &init_info ) )
    {
        AXION_LOG_INFO( Logger::Module::RHI, "GUI Backend Created Successfully" );
    } else
    {
        AXION_LOG_ERROR( Logger::Module::RHI, "Failed to initialize DX12 GUI Backend" );
    }

   
}

DX12GUIBackend::~DX12GUIBackend() {
    AXION_LOG_INFO( Logger::Module::RHI, "Destroying GUI Backend" );
    AXION_LOG_ASSERT( _platformShutdown, Logger::Module::RHI, "GUIBackend: Platform shutdown function not set!" );
    ImGui_ImplDX12_Shutdown();
    _platformShutdown();
    ImGui::DestroyContext();
    _gpuHeapSRV.free( _fontSlot ); // After ImGui shutdown: font texture no longer referenced
}

void DX12GUIBackend::newFrame() const {
    AXION_LOG_ASSERT( _platformNewFrame, Logger::Module::RHI, "GUIBackend: Platform new frame function not set!" );
    ImGui_ImplDX12_NewFrame();
    _platformNewFrame();
    ImGui::NewFrame();
}

void DX12GUIBackend::render( ICommandList* cmd ) const {
    ImGui::Render();

    ID3D12GraphicsCommandList* rawCmd = cmd->getNativeObject( RHI::ObjectTypes::DX12_CommandList );

    ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), rawCmd ); // Global heap already bound at begin()
}

} // namespace Graphics::RHI

AXION_NAMESPACE_END
