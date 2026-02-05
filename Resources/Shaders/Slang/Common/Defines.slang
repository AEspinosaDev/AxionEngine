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

public static const uint CANON_VERTEX_STRIDE = 64;

// PI
public static const float PI           = 3.141592653589793238462643;
public static const float PI_OVER_2     = PI / 2.0;
public static const float PI_OVER_4     = PI / 4.0;
public static const float INV_PI        = 1.0 / PI;
public static const float INV_PI_OVER_2 = 1.0 / PI_OVER_2;
public static const float INV_PI_OVER_4 = 1.0 / PI_OVER_4;
