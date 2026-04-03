#pragma once
#include "Nigiri/RHI/Common.h"

// Error Handle
#define VK_CHECK( x )                                                                 \
    do                                                                                \
    {                                                                                 \
        VkResult err = x;                                                             \
        if ( err )                                                                    \
        {                                                                             \
            NIGIRI_LOG_ERROR( Nigiri::Logger::Module::RHI, "Vulkan error: {}", err ); \
            Nigiri::Logger::flush();                                                  \
            abort();                                                                  \
        }                                                                             \
    } while ( 0 )