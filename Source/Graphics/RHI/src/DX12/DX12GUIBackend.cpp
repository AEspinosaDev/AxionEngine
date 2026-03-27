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
    ID3D12Device2* dx12Device = device->getNativeObject( ObjectTypes::DX12_Device );
    return Memory::makeOwned<DX12GUIBackend>( dx12Device,
                                    static_cast<DX12Device*>( device )->getQueue( QueueType::Graphics )->queue.Get(),
                                    desc );
}

DX12GUIBackend::DX12GUIBackend( ID3D12Device2* device, ID3D12CommandQueue* queue, const GUIBackendDesc& desc )
    : _desc( desc ) {

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.ConfigFlags |= desc.configFlags;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors             = 1;
    heapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    device->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &_guiHeap ) );

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
    init_info.Device                       = device;
    init_info.CommandQueue                 = queue;
    init_info.NumFramesInFlight            = _desc.framesInFlight;
    init_info.RTVFormat                    = DX12Translator::get( _desc.backbufferFormat );
    init_info.DSVFormat                    = DXGI_FORMAT_UNKNOWN; // Assuming no depth buffer for UI
    init_info.SrvDescriptorHeap            = _guiHeap.Get();
    init_info.LegacySingleSrvCpuDescriptor = _guiHeap->GetCPUDescriptorHandleForHeapStart();
    init_info.LegacySingleSrvGpuDescriptor = _guiHeap->GetGPUDescriptorHandleForHeapStart();

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

    ID3D12DescriptorHeap* heaps[] = { _guiHeap.Get() };
    rawCmd->SetDescriptorHeaps( 1, heaps );

    ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), rawCmd );
}

} // namespace Graphics::RHI

AXION_NAMESPACE_END
