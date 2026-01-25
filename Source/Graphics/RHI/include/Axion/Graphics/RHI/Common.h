#pragma once
// Axion Common Module
#include "Axion/Common/Defines.h"
#include "Axion/Common/Graphics/Defines.h"
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

#define ALIGN( _size, _alignment ) ( ( ( _size ) + ( _alignment ) - 1 ) & ~( ( _alignment ) - 1 ) )

AXION_NAMESPACE_BEGIN

namespace Graphics {

////////////////////////////////////////////////////////////////////////
// RHI Reserved Definitions
////////////////////////////////////////////////////////////////////////

namespace RHI {

struct DrawIndexedIndirectCommand {
    uint baseInstanceID; ///< Current Instance ID + offset

    uint indexCount;    ///< Number of indexes to draw
    uint instanceCount; ///< Number of instances to draw
    uint firstIndex;    ///< Offset in IndexBuffer (elements, not bytes)
    int  vertexOffset;  ///< Offset in VertexBuffer
    uint firstInstance; ///< ID as base

    uint _padding[2];
};

struct DispatchIndirectCommand {
    uint baseInstanceID;
    uint threadGroupCountX;
    uint threadGroupCountY;
    uint threadGroupCountZ;
};

enum class BarrierPolicy
{
    Auto, // CommandList checks and emits barriers if its necessary for utility functions (Seguro)
    None  // User has to manage barriers using RG or by hand (Speed for batching)
};

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
    ITexture*                 texture = nullptr;
    LoadOp                    loadOp  = LoadOp::Clear;
    std::optional<ClearValue> clearValue;
};

struct RenderingDesc {
    std::vector<RenderingAttachment> colorAttachments;
    RenderingAttachment              depthStencilAttachment;
    Extent2D                         renderArea;
};

enum class PipelineBindPoint : uchar
{
    None,
    Compute,
    Graphic,
    RTX
};

enum class AccelType
{
    BottomLevel, // BLAS: Geometry data (triangles, AABBs)
    TopLevel     // TLAS: Instances of BLAS
};

enum AccelBuildFlags : uchar
{
    ASBuildNone            = 0,
    ASBuildPreferFastTrace = 1 << 0, // Good for static geometry, slower build
    ASBuildPreferFastBuild = 1 << 1, // Good for dynamic geometry, faster build
    ASBuildAllowUpdate     = 1 << 2, // Allows refitting without full rebuild
    ASBuildMinimizeMemory  = 1 << 3
};

AXION_ENUM_CLASS_FLAG_OPERATORS( AccelBuildFlags )

enum class AccelInstanceFlags : uchar
{
    None                = 0,
    TriangleCullDisable = 0x1,
    ForceOpaque         = 0x4,
    ForceNonOpaque      = 0x8
};

AXION_ENUM_CLASS_FLAG_OPERATORS( AccelInstanceFlags )

enum class AccelPrimitive : uint
{
    Triangles,
    AABBs,
};

// Description for a single geometry piece (Mesh) inside a BLAS
struct AccelGeometryDesc {
    AccelPrimitive primitiveType;
    ulong          vertexBufferAddress;
    ulong          indexBufferAddress; //(optional)
    uint           vertexCount;
    uint           indexCount;
    uint           vertexStride; // Stride in bytes
    Format         vertexFormat;
    bool           isOpaque; // Optimization flag: no any-hit shader needed

    bool operator==( const AccelGeometryDesc& other ) const {
        return primitiveType == other.primitiveType &&
               vertexBufferAddress == other.vertexBufferAddress &&
               indexBufferAddress == other.indexBufferAddress &&
               vertexCount == other.vertexCount &&
               indexCount == other.indexCount &&
               vertexStride == other.vertexStride &&
               vertexFormat == other.vertexFormat &&
               isOpaque == other.isOpaque;
    }
    bool operator!=( const AccelGeometryDesc& other ) const {
        return !operator==( other );
    }
};

// Description for an instance inside a TLAS
struct AccelInstanceDesc {
    Math::Mat4         transform;
    uint               instanceID;          // Custom ID to access in shader (gl_InstanceCustomIndex)
    uint               instanceMask = 0xFF; // Visibility mask (0xFF usually)
    uint               hitGroupIndex;       // Offset in the Shader Binding Table
    AccelInstanceFlags flags;               // Instance specific flags
    ulong              blasDeviceAddress;   // The address of the BLAS this instance represents

    bool operator==( const AccelInstanceDesc& other ) const {
        return instanceID == other.instanceID &&
               instanceMask == other.instanceMask &&
               hitGroupIndex == other.hitGroupIndex &&
               flags == other.flags &&
               blasDeviceAddress == other.blasDeviceAddress;
    }
    bool operator!=( const AccelInstanceDesc& other ) const {
        return !operator==( other );
    }
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
constexpr ObjectType DX12_StateObject                = 0x0001000d;
constexpr ObjectType DX12_StateProps                 = 0x0001000e;

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
