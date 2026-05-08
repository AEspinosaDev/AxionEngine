#pragma once
// Axion Common Module
#include "Axion/Common/Common.h"
#include "Axion/Common/Containers/Array.h"
#include "Axion/Common/Containers/STLWrapper/Maps.h"
#include "Axion/Common/Containers/STLWrapper/String.h"
#include "Axion/Common/Containers/STLWrapper/Vector.h"
#include "Axion/Common/Containers/SmallVector.h"
#include "Axion/Common/Containers/String.h"
#include "Axion/Common/Containers/Vector.h"
#include "Axion/Common/Graphics/Common.h"
#include "Axion/Common/Logging.h"
#include "Axion/Common/Math.h"
#include "Axion/Common/Memory/Pointers/OwnerPtr.h"
#include <Axion/Common/Memory/Allocators/SubAllocators/FreeListSubAllocator.h>
#include <Axion/Common/Memory/Allocators/SubAllocators/LinearSubAllocator.h>

#define ALIGN( _size, _alignment ) ( ( ( _size ) + ( _alignment ) - 1 ) & ~( ( _alignment ) - 1 ) )
#define AXION_TEXTURE_DATA_PLACEMENT_ALIGNMENT ( 512 )

AXION_NAMESPACE_BEGIN

namespace Graphics {

////////////////////////////////////////////////////////////////////////
// Device Buffer Sub Allocator
////////////////////////////////////////////////////////////////////////

namespace RHI {
class IBuffer;
}

/**
 * A memory slice allocated from a larger buffer, used for sub-allocations within the RHI.
 * This struct abstracts the details of the underlying buffer and provides necessary information for both CPU and GPU access.
 */
using BufferSlice = Memory::SubAllocation<RHI::IBuffer>;
/** @brief Free-list allocator for persistent buffer data without thread synchronization. */
template <typename VisibilityPolicy = Memory::VisibilityShared>
using BufferFreeListAllocator = Memory::FreeListSubAllocator<RHI::IBuffer, VisibilityPolicy, NoLockPolicy>;

/** @brief Free-list allocator for persistent, device-local (GPU-only) buffer data without thread synchronization. */
using BufferGPUFreeListAllocator = Memory::FreeListSubAllocator<RHI::IBuffer, Memory::VisibilityDeviceOnly, NoLockPolicy>;

/** @brief Thread-safe free-list allocator for persistent buffer data. */
template <typename VisibilityPolicy = Memory::VisibilityShared>
using LockedBufferFreeListAllocator = Memory::FreeListSubAllocator<RHI::IBuffer, VisibilityPolicy, MutexLockPolicy>;

/** @brief Linear allocator for transient buffer data without thread synchronization. Ideal for per-frame allocations. */
template <typename VisibilityPolicy = Memory::VisibilityShared>
using BufferLinearAllocator = Memory::LinearSubAllocator<RHI::IBuffer, VisibilityPolicy, NoLockPolicy>;

/** @brief Linear allocator for transient, device-local (GPU-only) buffer data without thread synchronization. */
using BufferGPULinearAllocator = Memory::LinearSubAllocator<RHI::IBuffer, Memory::VisibilityDeviceOnly, NoLockPolicy>;

/** @brief Thread-safe linear allocator for transient buffer data. */
template <typename VisibilityPolicy = Memory::VisibilityShared>
using LockedBufferLinearAllocator = Memory::LinearSubAllocator<RHI::IBuffer, VisibilityPolicy, MutexLockPolicy>;

////////////////////////////////////////////////////////////////////////
// RHI Reserved Definitions
////////////////////////////////////////////////////////////////////////

namespace RHI {

struct DrawIndexedIndirectCommand {
    u32 baseInstanceID; ///< Current Instance ID + offset

    u32 indexCount;    ///< Number of indexes to draw
    u32 instanceCount; ///< Number of instances to draw
    u32 firstIndex;    ///< Offset in IndexBuffer (elements, not bytes)
    int vertexOffset;  ///< Offset in VertexBuffer
    u32 firstInstance; ///< ID as base

    u32 _padding[2];
};

struct DispatchIndirectCommand {
    u32 baseInstanceID;
    u32 threadGroupCountX;
    u32 threadGroupCountY;
    u32 threadGroupCountZ;
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

enum class FeatureType : u16
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

enum class ResourceState : u32
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

enum class FormatSupport : u32
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

enum class DescriptorType : byte
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
    STLW::Vector<RenderingAttachment> colorAttachments;
    RenderingAttachment               depthStencilAttachment;
    Extent2D                          renderArea;
};

enum class PipelineBindPoint : byte
{
    None,
    Compute,
    Graphic,
    RTX,
    Mesh
};

enum class AccelType
{
    BottomLevel, // BLAS: Geometry data (triangles, AABBs)
    TopLevel     // TLAS: Instances of BLAS
};

enum AccelBuildFlags : byte
{
    ASBuildNone            = 0,
    ASBuildPreferFastTrace = 1 << 0, // Good for static geometry, slower build
    ASBuildPreferFastBuild = 1 << 1, // Good for dynamic geometry, faster build
    ASBuildAllowUpdate     = 1 << 2, // Allows refitting without full rebuild
    ASBuildMinimizeMemory  = 1 << 3
};

AXION_ENUM_CLASS_FLAG_OPERATORS( AccelBuildFlags )

enum class AccelInstanceFlags : byte
{
    None                = 0,
    TriangleCullDisable = 0x1,
    ForceOpaque         = 0x4,
    ForceNonOpaque      = 0x8
};

AXION_ENUM_CLASS_FLAG_OPERATORS( AccelInstanceFlags )

enum class AccelPrimitive : u32
{
    Triangles,
    AABBs,
};

// Description for a single geometry piece (Mesh) inside a BLAS
struct AccelGeometryDesc {
    AccelPrimitive primitiveType;
    u64            vertexBufferAddress;
    u64            indexBufferAddress; //(optional)
    u32            vertexCount;
    u32            indexCount;
    u32            vertexStride; // Stride in bytes
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
    u32                instanceID;          // Custom ID to access in shader (gl_InstanceCustomIndex)
    u32                instanceMask = 0xFF; // Visibility mask (0xFF usually)
    u32                hitGroupIndex;       // Offset in the Shader Binding Table
    AccelInstanceFlags flags;               // Instance specific flags
    u64                blasDeviceAddress;   // The address of the BLAS this instance represents

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

typedef u32 ObjectType;

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
    u64   integer;
    void* pointer;

    NativeObject( u64 i )
        : integer( i ) {}
    NativeObject( void* p )
        : pointer( p ) {}
    NativeObject( u64 i, void* p )
        : integer( i )
        , pointer( p ) {}

    template <typename T>
    operator T*() const { return static_cast<T*>( pointer ); }
};

/**
 * @brief Base interface for all device-related objects (Device, Swapchain, CommandList, Texture, etc.)
 * Provides common functionality like debug naming and native object access.
 */
class IDeviceObject
{
protected:
    IDeviceObject()          = default;
    virtual ~IDeviceObject() = default;

public:
    virtual void             setDebugName( std::string_view name ) = 0;
    virtual std::string_view getDebugName() const                  = 0;
    virtual STLW::String     toString() const                      = 0;

    // Returns a native object or interface, for example ID3D12Device*, or nullptr if the requested interface is unavailable.
    // Does *not* AddRef the returned interface.
    virtual NativeObject getNativeObject( ObjectType objectType ) {
        (void)objectType;
        return nullptr;
    }

    AXION_DISABLE_MOVE( IDeviceObject );
    AXION_DISABLE_COPY( IDeviceObject )
};

} // namespace RHI
} // namespace Graphics
AXION_NAMESPACE_END
