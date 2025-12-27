#ifdef __D3D12__
    // DX12: Wrapper + Control Manual
    #define PUSH_CONSTANT(Type, Name, Reg, Space) \
        ConstantBuffer<Type> Name : register(Reg, Space)
#else
    // VULKAN: Keyword nativa
    #define PUSH_CONSTANT(Type, Name, Reg, Space) \
        [[vk::push_constant]] \
        Type Name
#endif