#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Pipeline.h"
#include "Axion/Graphics/RHI/Resource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

/**
 * @brief Data container defining the structure and content of a Shader Binding Table.
 * Acts as a CPU-side description before uploading to the GPU.
 */
struct ShaderBindingTable {

    /** @brief Represents a single shader record (Shader ID + Root Arguments). */
    struct Record {
        std::string shaderName; ///< Export name used to retrieve the Shader Identifier.
        void*       rootArgs;   ///< Pointer to local root arguments (constants/descriptors).
        uint        argsSize;   ///< Size of the root arguments in bytes.
    };

    /** @brief Describes the GPU memory layout of an uploaded SBT, used for DispatchRays. */
    struct BufferView {
        ulong rayGenAddress; ///< GPU Virtual Address of the Ray Generation record.
        struct Region {
            ulong startAddress;  ///< GPU Virtual Address of the table region.
            uint  sizeInBytes;   ///< Total size of the region.
            uint  strideInBytes; ///< Stride between records in this region.
        } missRegion, hitRegion, callableRegion;
    };

    Record              rayGen;     ///< The Ray Generation shader record (only one allowed).
    std::vector<Record> missGroups; ///< List of Miss shader records.
    std::vector<Record> hitGroups;  ///< List of Hit Group records.
    std::vector<Record> callables;  ///< Optional list of Callable shader records.

    /// @brief Sets the mandatory Ray Generation shader.
    inline void setRayGen( const std::string& name, void* args = nullptr, uint size = 0 ) {
        rayGen = { name, args, size };
    }
    /// @brief Adds a Miss shader record.
    inline void addMiss( const std::string& name, void* args = nullptr, uint size = 0 ) {
        missGroups.push_back( { name, args, size } );
    }
    /// @brief Adds a Hit Group record (Closest Hit + Any Hit + Intersection).
    inline void addHitGroup( const std::string& name, void* args = nullptr, uint size = 0 ) {
        hitGroups.push_back( { name, args, size } );
    }
    /// @brief Adds a Callable shader record.
    inline void addCallable( const std::string& name, void* args = nullptr, uint size = 0 ) {
        callables.push_back( { name, args, size } );
    }
};

typedef ShaderBindingTable SBT;


DEFINE_COM_PTR_FOR_TYPE( ISBTAllocator, SBTAllocator )

/**
 * @brief Interface for a linear allocator specialized in managing Shader Binding Table memory.
 * Handles alignment and uploading of SBT records to the GPU.
 */
class ISBTAllocator : public IResource
{
public:
    struct Description {
        uint        sizeInBytes;
        std::string debugName;
    };

    virtual ~ISBTAllocator() = default;

    /**
     * @brief Allocates and uploads the SBT data to the GPU.
     * @param sbt The CPU-side description of the table.
     * @param pip The pipeline used to retrieve Shader Identifiers.
     * @return The buffer view required for the DispatchRays command.
     */
    virtual SBT::BufferView allocate( const ShaderBindingTable& sbt, IRayTracingPipeline* pip ) = 0;

    /** @brief Returns the allocator description. */
    virtual const Description& getDescription() const = 0;

    /** @brief Resets the internal offset. Must be called at the start of the frame. */
    virtual void reset() = 0;
};

typedef ISBTAllocator::Description SBTAllocatorDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END