#pragma once
// Nigiri Common Module
#include "Axion/Common/Defines.h"
#include "Axion/Common/Logging.h"
#include "Axion/Common/Math.h"

// DirectX 12
using namespace Microsoft::WRL;

#include <directx/d3dx12.h> // D3D12 extension library.

#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#pragma comment( lib, "dxguid.lib" )

// Vulkan
#include <vulkan/vulkan.h>
// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

AXION_NAMESPACE_BEGIN

namespace Graphics {

enum class API : uint
{
    DirectX12,
    Vulkan
};

enum class PlatformType : uint
{
    Win32,
    GLFW,
    SDL,
    Invalid
};

enum class PresentMode : uint
{
    Immediate,
    Vsync
};

enum class BufferingType : uint
{
    Mono   = 1,
    Double = 2,
    Triple = 3,
};

enum class Format : uchar
{
    UNKNOWN,

    R8_UINT,
    R8_SINT,
    R8_UNORM,
    R8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    R16_UINT,
    R16_SINT,
    R16_UNORM,
    R16_SNORM,
    R16_FLOAT,
    BGRA4_UNORM,
    B5G6R5_UNORM,
    B5G5R5A1_UNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_UNORM,
    RGBA8_SNORM,
    BGRA8_UNORM,
    BGRX8_UNORM,
    SRGBA8_UNORM,
    SBGRA8_UNORM,
    SBGRX8_UNORM,
    R10G10B10A2_UNORM,
    R11G11B10_FLOAT,
    RG16_UINT,
    RG16_SINT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_FLOAT,
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,

    D16,
    D24S8,
    X24G8_UINT,
    D32,
    D32S8,
    X32G8_UINT,

    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_UNORM_SRGB,

    COUNT,
};

constexpr size_t getFormatBytes( Format format ) {
    switch ( format )
    {
        // 8-bit
        case Format::R8_UINT:
        case Format::R8_SINT:
        case Format::R8_UNORM:
        case Format::R8_SNORM:
            return 1;

        // RG8
        case Format::RG8_UINT:
        case Format::RG8_SINT:
        case Format::RG8_UNORM:
        case Format::RG8_SNORM:
            return 2;

        // 16-bit
        case Format::R16_UINT:
        case Format::R16_SINT:
        case Format::R16_UNORM:
        case Format::R16_SNORM:
        case Format::R16_FLOAT:
            return 2;

        // 16-bit packed
        case Format::BGRA4_UNORM:
        case Format::B5G6R5_UNORM:
        case Format::B5G5R5A1_UNORM:
            return 2;

        // RGBA8 / 8-bit per channel
        case Format::RGBA8_UINT:
        case Format::RGBA8_SINT:
        case Format::RGBA8_UNORM:
        case Format::RGBA8_SNORM:
        case Format::BGRA8_UNORM:
        case Format::BGRX8_UNORM:
        case Format::SRGBA8_UNORM:
        case Format::SBGRA8_UNORM:
        case Format::SBGRX8_UNORM:
            return 4;

        // HDR-ish
        case Format::R10G10B10A2_UNORM:
        case Format::R11G11B10_FLOAT:
            return 4;

        // RG16
        case Format::RG16_UINT:
        case Format::RG16_SINT:
        case Format::RG16_UNORM:
        case Format::RG16_SNORM:
        case Format::RG16_FLOAT:
            return 4;

        // 32-bit single
        case Format::R32_UINT:
        case Format::R32_SINT:
        case Format::R32_FLOAT:
            return 4;

        // RGBA16
        case Format::RGBA16_UINT:
        case Format::RGBA16_SINT:
        case Format::RGBA16_FLOAT:
        case Format::RGBA16_UNORM:
        case Format::RGBA16_SNORM:
            return 8;

        // RG32
        case Format::RG32_UINT:
        case Format::RG32_SINT:
        case Format::RG32_FLOAT:
            return 8;

        // RGB32
        case Format::RGB32_UINT:
        case Format::RGB32_SINT:
        case Format::RGB32_FLOAT:
            return 12;

        // RGBA32
        case Format::RGBA32_UINT:
        case Format::RGBA32_SINT:
        case Format::RGBA32_FLOAT:
            return 16;

        // Depth / stencil
        case Format::D16:
            return 2;
        case Format::D24S8:
            return 4;
        case Format::D32:
            return 4;
        case Format::D32S8:
            return 8;
        case Format::X24G8_UINT:
            return 4;
        case Format::X32G8_UINT:
            return 8;

        // BC compressed (block-compressed: 4x4 blocks, size in bytes per block)
        case Format::BC1_UNORM:
        case Format::BC1_UNORM_SRGB:
        case Format::BC4_UNORM:
        case Format::BC4_SNORM:
            return 8; // bytes per 4x4 block
        case Format::BC2_UNORM:
        case Format::BC2_UNORM_SRGB:
        case Format::BC3_UNORM:
        case Format::BC3_UNORM_SRGB:
        case Format::BC5_UNORM:
        case Format::BC5_SNORM:
        case Format::BC6H_UFLOAT:
        case Format::BC6H_SFLOAT:
        case Format::BC7_UNORM:
        case Format::BC7_UNORM_SRGB:
            return 16; // bytes per 4x4 block

        default:
            return 0;
    }
}

enum class TextureDimension : uchar
{
    Unknown,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    TextureCube,
    TextureCubeArray,
    Texture2DMS,
    Texture2DMSArray,
    Texture3D

};

// View type flags (bitmask)
enum TextureViewFlags : uint
{
    TextureViewNone            = 0,
    TextureViewShaderResource  = 1 << 0, // SRV in DX12, VK_IMAGE_VIEW_TYPE_*
    TextureViewRenderTarget    = 1 << 1, // RTV / VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    TextureViewDepthStencil    = 1 << 2, // DSV / VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    TextureViewUnorderedAccess = 1 << 3  // UAV / VK_IMAGE_USAGE_STORAGE_BIT
};

AXION_ENUM_CLASS_FLAG_OPERATORS( TextureViewFlags )

enum BufferViewFlags : uint
{
    BufferViewNone            = 0,
    BufferViewShaderResource  = 1 << 0, // SRV / VK_DESCRIPTOR_TYPE_STORAGE_BUFFER/UNIFORM_TEXEL
    BufferViewUnorderedAccess = 1 << 1, // UAV / VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    BufferViewConstantBuffer  = 1 << 2  // CBV / VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
};

AXION_ENUM_CLASS_FLAG_OPERATORS( BufferViewFlags )

enum class MemoryUsage : uint
{
    Unknown    = 0,
    CPUVisible = 1 << 0, // Upload / staging
    GPUOnly    = 1 << 1,
    Readback   = 1 << 2
};
AXION_ENUM_CLASS_FLAG_OPERATORS( MemoryUsage )

enum class BufferUsage : uint
{
    None        = 0,
    Vertex      = 1 << 0,
    Index       = 1 << 1,
    Uniform     = 1 << 2, // CB / UBO
    Storage     = 1 << 3, // UAV / SSBO
    Indirect    = 1 << 4,
    TransferSrc = 1 << 5,
    TransferDst = 1 << 6,
};
AXION_ENUM_CLASS_FLAG_OPERATORS( BufferUsage )

enum class Filter
{
    Nearest,
    Linear
};
enum class AddressMode
{
    Repeat,
    Wrap,
    Clamp,
    Mirror,
    Border
};

struct ClearValue {
    Math::Vec4 color   = { 0.0, 0.0, 0.0, 1.0 }; // RGBA for RTV/UAV
    float      depth   = 1.0f;                   // depth for DSV
    uchar      stencil = 0;                      // stencil for DSV
};

////////////////////////////////////////////////////////////////////////
// RHI Reserved Definitions
////////////////////////////////////////////////////////////////////////

namespace RHI {

enum class FenceType
{
    Default,           // D3D12_FENCE_FLAG_NONE  →  timeline semaphore
    Shared,            // D3D12_FENCE_FLAG_SHARED → exportable timeline semaphore
    CrossAdapter,      // D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER → device-group external semaphore
    GPUOnlyLightweight // D3D12_FENCE_FLAG_NON_MONITORED → binary semaphore
};
enum class QueueType
{
    Graphics = 0,
    Compute  = 1,
    Transfer = 2
};

enum class Feature : ushort
{
    ComputeQueue,
    ConservativeRasterization,
    ConstantBufferRanges,
    CopyQueue,
    DeferredCommandLists,
    FastGeometryShader,
    HeapDirectlyIndexed,
    HlslExtensionUAV,
    LinearSweptSpheres,
    Meshlets,
    RayQuery,
    RayTracingAccelStruct,
    RayTracingClusters,
    RayTracingOpacityMicromap,
    RayTracingPipeline,
    SamplerFeedback,
    ShaderExecutionReordering,
    ShaderSpecializations,
    SinglePassStereo,
    Spheres,
    VariableRateShading,
    VirtualResources,
    WaveLaneCountMinMax,
    CooperativeVectorInferencing,
    CooperativeVectorTraining
};

enum class ResourceState : uint
{
    Undefined = 0,

    // Common usage
    Common      = 1 << 0,
    GeneralRead = 1 << 1,

    // Buffers
    VertexBuffer     = 1 << 2,
    IndexBuffer      = 1 << 3,
    ConstantBuffer   = 1 << 4,
    IndirectArgument = 1 << 5,

    // Shader resource
    ShaderResource         = 1 << 6, // generic SRV
    PixelShaderResource    = 1 << 7,
    NonPixelShaderResource = 1 << 8,

    // UAV
    UnorderedAccess = 1 << 9,

    // Render targets & depth
    RenderTarget = 1 << 10,
    DepthWrite   = 1 << 11,
    DepthRead    = 1 << 12,

    // Copy / Transfer
    CopySource    = 1 << 13,
    CopyDest      = 1 << 14,
    ResolveSource = 1 << 15,
    ResolveDest   = 1 << 16,

    // Present
    Present = 1 << 17,

    // Raytracing
    RaytracingAS = 1 << 18,

    // Shading rate image (VRS)
    ShadingRateSource = 1 << 19,

    // Video (optional, DX12-specific)
    VideoDecodeRead   = 1 << 20,
    VideoDecodeWrite  = 1 << 21,
    VideoProcessRead  = 1 << 22,
    VideoProcessWrite = 1 << 23,
    VideoEncodeRead   = 1 << 24,
    VideoEncodeWrite  = 1 << 25,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( ResourceState )

enum class FormatSupport : uint
{
    None = 0,

    Buffer       = 0x00000001,
    IndexBuffer  = 0x00000002,
    VertexBuffer = 0x00000004,

    Texture      = 0x00000008,
    DepthStencil = 0x00000010,
    RenderTarget = 0x00000020,
    Blendable    = 0x00000040,

    ShaderLoad     = 0x00000080,
    ShaderSample   = 0x00000100,
    ShaderUavLoad  = 0x00000200,
    ShaderUavStore = 0x00000400,
    ShaderAtomic   = 0x00000800,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( FormatSupport )

// Shader type mask. The values match ones used in Vulkan.
enum class ShaderType : ushort
{
    None = 0x0000,

    Compute = 0x0020,

    Vertex        = 0x0001,
    Hull          = 0x0002,
    Domain        = 0x0004,
    Geometry      = 0x0008,
    Pixel         = 0x0010,
    Amplification = 0x0040,
    Mesh          = 0x0080,
    AllGraphics   = 0x00DF,

    RayGeneration = 0x0100,
    AnyHit        = 0x0200,
    ClosestHit    = 0x0400,
    Miss          = 0x0800,
    Intersection  = 0x1000,
    Callable      = 0x2000,
    AllRayTracing = 0x3F00,

    All = 0x3FFF,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( ShaderType )

enum class PrimitiveTopology : uchar
{
    Undefined = 0,
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    PatchList
};

enum class FillMode : uchar
{
    Solid     = 0,
    Wireframe = 1
};

enum class CullMode : uchar
{
    None  = 0,
    Front = 1,
    Back  = 2
};

enum class CompareOp : uchar
{
    Never        = 0,
    Less         = 1,
    Equal        = 2,
    LessEqual    = 3,
    Greater      = 4,
    NotEqual     = 5,
    GreaterEqual = 6,
    Always       = 7
};

enum class BlendFactor : uchar
{
    Zero = 0,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    SrcAlphaSaturate
};

enum class BlendOp : uchar
{
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

enum class DescriptorType : uchar
{
    UniformBuffer = 0,     // Constant buffer / UBO
    StorageBuffer,         // RW buffer / SSBO
    ReadonlyStorageBuffer, // RO buffer / SSBO
    SampledImage,          // Texture SRV
    StorageImage,          // RW texture / UAV
    Sampler,               // Sampler object
    AccelerationStructure, // For raytracing
    CombinedImageSampler   // For Vulkan convenience (DX12 splits)
};

enum class ShaderStage : uint8_t
{
    None     = 0,
    Vertex   = 1 << 0,
    Pixel    = 1 << 1,
    Hull     = 1 << 2,
    Domain   = 1 << 3,
    Geometry = 1 << 4,
    Compute  = 1 << 5,
    Mesh     = 1 << 6,
    All      = Vertex | Pixel | Hull | Domain | Geometry | Compute | Mesh
};

AXION_ENUM_CLASS_FLAG_OPERATORS( ShaderStage )

enum class LoadOp
{
    Load,
    Clear,
    Discard
};

class ITexture;
struct RenderingAttachment {
    ITexture*  texture    = nullptr;
    LoadOp     loadOp     = LoadOp::Clear;
    ClearValue clearValue = {};
};

struct RenderingDesc {
    std::vector<RenderingAttachment> colorAttachments;
    RenderingAttachment              depthStencilAttachment;
    Extent2D                         renderArea;
};

typedef uint ObjectType;

// ObjectTypes namespace contains identifiers for various object types.
// All constants have to be distinct. Implementations may extend the list.
//
// The encoding is chosen to minimize potential conflicts between implementations.
// 0x00aabbcc, where:
//   aa is GAPI, 1 for DX12, 2 for VK
//   bb is layer, 0 for native GAPI objects, 1 for reference backend, 2 for user-defined backends
//   cc is a sequential number

namespace ObjectTypes {

constexpr ObjectType WIN32_WINDOW = 0x00000002;
constexpr ObjectType GLFW_Window  = 0x00000001;

constexpr ObjectType DX12_Device                     = 0x00010001;
constexpr ObjectType DX12_CommandQueue               = 0x00010002;
constexpr ObjectType DX12_CommandList                = 0x00010003;
constexpr ObjectType DX12_Resource                   = 0x00010004;
constexpr ObjectType DX12_RenderTargetViewDescriptor = 0x00010005;
constexpr ObjectType DX12_CommandAllocator           = 0x00010006;
constexpr ObjectType DX12_SwapChain                  = 0x00010007;
constexpr ObjectType DX12_PipelineState              = 0x00010008;
constexpr ObjectType DX12_RootSignature              = 0x00010009;
constexpr ObjectType DX12_DescriptorHeap             = 0x0001000a;
constexpr ObjectType DX12_DescriptorSet              = 0x0001000b;
constexpr ObjectType DX12_DescriptorSamplerHeap      = 0x0001000c;

constexpr ObjectType VK_Device                   = 0x00020001;
constexpr ObjectType VK_PhysicalDevice           = 0x00020002;
constexpr ObjectType VK_Instance                 = 0x00020003;
constexpr ObjectType VK_Queue                    = 0x00020004;
constexpr ObjectType VK_CommandBuffer            = 0x00020005;
constexpr ObjectType VK_DeviceMemory             = 0x00020006;
constexpr ObjectType VK_Buffer                   = 0x00020007;
constexpr ObjectType VK_Image                    = 0x00020008;
constexpr ObjectType VK_ImageView                = 0x00020009;
constexpr ObjectType VK_AccelerationStructureKHR = 0x0002000a;
constexpr ObjectType VK_Sampler                  = 0x0002000b;
constexpr ObjectType VK_ShaderModule             = 0x0002000c;
constexpr ObjectType VK_RenderPass               = 0x0002000d;
constexpr ObjectType VK_Framebuffer              = 0x0002000e;
constexpr ObjectType VK_DescriptorPool           = 0x0002000f;
constexpr ObjectType VK_DescriptorSetLayout      = 0x00020010;
constexpr ObjectType VK_DescriptorSet            = 0x00020011;
constexpr ObjectType VK_PipelineLayout           = 0x00020012;
constexpr ObjectType VK_Pipeline                 = 0x00020013;
constexpr ObjectType VK_Micromap                 = 0x00020014;
constexpr ObjectType VK_ImageCreateInfo          = 0x00020015;

}; // namespace ObjectTypes

struct NativeObject {
    ulong integer;
    void* pointer;

    NativeObject( ulong i )
        : integer( i ) {}
    NativeObject( void* p )
        : pointer( p ) {}
    NativeObject( ulong i, void* p )
        : integer( i )
        , pointer( p ) {}

    template <typename T>
    operator T*() const { return static_cast<T*>( pointer ); }
};

class IResource
{
protected:
    IResource()          = default;
    virtual ~IResource() = default;

public:
    // Intrusive ref count API
    virtual ulong addRef() noexcept            = 0;
    virtual ulong release() noexcept           = 0;
    virtual ulong getRefCount() const noexcept = 0;

    // Debug utilities (optional but very useful for graphics engines)
    virtual void               setDebugName( const std::string& name ) = 0;
    virtual const std::string& getDebugName() const                    = 0;
    virtual std::string        toString() const                        = 0;

    // Returns a native object or interface, for example ID3D12Device*, or nullptr if the requested interface is unavailable.
    // Does *not* AddRef the returned interface.
    virtual NativeObject getNativeObject( ObjectType objectType ) {
        (void)objectType;
        return nullptr;
    }

    // Non-copyable, non-movable
    IResource( const IResource& )            = delete;
    IResource& operator=( const IResource& ) = delete;
    IResource( IResource&& )                 = delete;
    IResource& operator=( IResource&& )      = delete;
};

// Template to add reference counting to any base
template <class T>
class RefCounter : public T
{
public:
    RefCounter()
        : _refCount( 1 ) {
        // std::cout << "[RefCounter] Created: " << this << " RefCount=1\n";
    }

    virtual ~RefCounter() {
        // std::cout << "[RefCounter] Destroyed: " << this << "\n";
    }

    ulong addRef() noexcept override {
        ulong val = ++_refCount;
        // std::cout << "[RefCounter] addRef: " << this << " RefCount=" << val << "\n";
        return val;
    }

    ulong release() noexcept override {
        ulong val = --_refCount;
        // std::cout << "[RefCounter] release: " << this << " RefCount=" << val << "\n";
        if ( val == 0 )
        {
            // std::cout << "[RefCounter] deleting: " << this << "\n";
            delete this;
        }
        return val;
    }

    ulong getRefCount() const noexcept override {
        return _refCount.load();
    }

private:
    std::atomic<ulong> _refCount;
};

// COM-style smart pointer
template <class T>
class Ptr
{
public:
    Ptr()
        : _ptr( nullptr ) {}
    Ptr( std::nullptr_t )
        : _ptr( nullptr ) {}

    Ptr( T* raw )
        : _ptr( raw ) {
        internalAddRef();
    }

    Ptr( const Ptr& other )
        : _ptr( other._ptr )
        , _ownsReference( true ) {
        internalAddRef();
    }
    Ptr( Ptr&& other ) noexcept
        : _ptr( other._ptr )
        , _ownsReference( other._ownsReference ) {
        other._ptr           = nullptr;
        other._ownsReference = false;
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    Ptr( const Ptr<U>& other )
        : _ptr( other._ptr ) {
        internalAddRef();
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    Ptr( Ptr<U>&& other ) noexcept
        : _ptr( other._ptr ) {
        other._ptr = nullptr;
    }
    Ptr( T* raw, bool takeOwnership )
        : _ptr( raw )
        , _ownsReference( takeOwnership ) // Nuevo flag
    {
        if ( _ownsReference )
        {
            internalAddRef();
        }
    }

    ~Ptr() {
        if ( _ownsReference )
        {
            internalRelease();
        }
    }

    Ptr& operator=( const Ptr& other ) {
        if ( this != &other )
        {
            internalRelease();
            _ptr = other._ptr;
            internalAddRef();
        }
        return *this;
    }

    // operators
    T* operator->() const { return _ptr; }
    T& operator*() const { return *_ptr; }
       operator bool() const { return _ptr != nullptr; }
       operator T*() const { return _ptr; }

    T* get() const { return _ptr; }

    // Returns a pointer to the internal pointer (like COM & operator)
    T** operator&() {
        internalRelease();
        _ptr = nullptr;
        return &_ptr;
    }

    // Detach the pointer (caller takes ownership, RefPtr forgets it)
    T* detach() {
        T* tmp = _ptr;
        _ptr   = nullptr;
        return tmp;
    }

    // Attach a raw pointer (takes ownership)
    void attach( T* raw ) {
        internalRelease();
        _ptr = raw;
    }

    // Factory method, returns Ptr that owns new object
    template <class... Args>
    static Ptr<T> create( Args&&... args ) {
        T* obj = new T( std::forward<Args>( args )... );
        return Ptr<T>( obj );
    }

private:
    void internalAddRef() {
        if ( _ptr )
            _ptr->addRef();
    }

    void internalRelease() {
        if ( _ptr )
            _ptr->release();
        _ptr = nullptr;
    }

private:
    T*   _ptr;
    bool _ownsReference = true;

    template <typename>
    friend class Ptr;
};

#define DEFINE_COM_PTR_FOR_TYPE( type, clean ) \
    class type;                                \
    typedef Ptr<type> clean##Ptr;

} // namespace RHI
} // namespace Graphics
AXION_NAMESPACE_END
