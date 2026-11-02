#pragma once
// HEADER: preprocessor only (macros don't cross 'import'). #include it, never import it.

#ifdef __D3D12__
// DX12: Wrapper + Control Manual
#define PUSH_CONSTANT( Type, Name, Reg, Space ) \
    ConstantBuffer<Type> Name : register( Reg, Space )
#else
// VULKAN: Keyword nativa
#define PUSH_CONSTANT( Type, Name, Reg, Space ) \
    [[vk::push_constant]]                       \
    Type Name
#endif

// ============================================================================
// GLOBAL LAYOUT SPACES / REGISTERS (must match RasterizerConfig.h & Graphics::PassIO)
// ============================================================================

#define SPACE_PERSISTENT space0
#define SPACE_FRAME      space1
#define SPACE_PASS       space2
#define SPACE_PUSH       space3

// Push constant register (use with PUSH_CONSTANT( Type, Name, PUSH_CONSTANT_REG, SPACE_PUSH ))
#define PUSH_CONSTANT_REG b0

// Space 2 slot registers
#define PASS_SRV_IMAGE_0 t0
#define PASS_SRV_IMAGE_1 t1
#define PASS_SRV_IMAGE_2 t2
#define PASS_SRV_IMAGE_3 t3
#define PASS_SRV_IMAGE_4 t4
#define PASS_SRV_IMAGE_5 t5
#define PASS_SRV_IMAGE_6 t6
#define PASS_SRV_IMAGE_7 t7

#define PASS_SRV_BUFFER_0 t8
#define PASS_SRV_BUFFER_1 t9
#define PASS_SRV_BUFFER_2 t10
#define PASS_SRV_BUFFER_3 t11
#define PASS_SRV_BUFFER_4 t12
#define PASS_SRV_BUFFER_5 t13
#define PASS_SRV_BUFFER_6 t14
#define PASS_SRV_BUFFER_7 t15

#define PASS_UAV_IMAGE_0 u0
#define PASS_UAV_IMAGE_1 u1
#define PASS_UAV_IMAGE_2 u2
#define PASS_UAV_IMAGE_3 u3
#define PASS_UAV_IMAGE_4 u4
#define PASS_UAV_IMAGE_5 u5
#define PASS_UAV_IMAGE_6 u6
#define PASS_UAV_IMAGE_7 u7

#define PASS_UAV_BUFFER_0 u8
#define PASS_UAV_BUFFER_1 u9
#define PASS_UAV_BUFFER_2 u10
#define PASS_UAV_BUFFER_3 u11
#define PASS_UAV_BUFFER_4 u12
#define PASS_UAV_BUFFER_5 u13
#define PASS_UAV_BUFFER_6 u14
#define PASS_UAV_BUFFER_7 u15

// ============================================================================
// PASS I/O DECLARATIONS (space 2 of the global layout). Read-only: DECLARE_TEXTURE/BUFFER, read-write: DECLARE_RW_*.
// Slot = index inside each range [0, 7]
// ============================================================================
// Usage: DECLARE_TEXTURE( Texture2D<uint2>, gVisBuffer, 0 );

#define DECLARE_TEXTURE( Type, Name, Slot )    Type Name : register( PASS_SRV_IMAGE_##Slot, SPACE_PASS )
#define DECLARE_BUFFER( Type, Name, Slot )     Type Name : register( PASS_SRV_BUFFER_##Slot, SPACE_PASS )
#define DECLARE_RW_TEXTURE( Type, Name, Slot ) Type Name : register( PASS_UAV_IMAGE_##Slot, SPACE_PASS )
#define DECLARE_RW_BUFFER( Type, Name, Slot )  Type Name : register( PASS_UAV_BUFFER_##Slot, SPACE_PASS )
#define DECLARE_PUSH_CONSTANT( Type, Name )    PUSH_CONSTANT( Type, Name, PUSH_CONSTANT_REG, SPACE_PUSH )
