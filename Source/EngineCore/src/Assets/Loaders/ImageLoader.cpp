#include "Loaders.h"
#include <stb_image.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

bool loadImage( const STLW::String& filepath, ImageData& outImage, TextureImportFlags flags ) {

    int w, h, c;
    
    stbi_set_flip_vertically_on_load( flags & TextureImportFlipVertically );
    int req_comp = (flags & TextureImportForce4Channels) ? 4 : 0;

    if ( stbi_is_hdr( filepath.c_str() ) )
    {
        outImage.isHDR = true;
        float* data = stbi_loadf( filepath.c_str(), &w, &h, &c, req_comp );

        if ( !data ) {
            AXION_LOG_ERROR( Logger::Module::Core, "Failed to load HDR image: {}", filepath );
            return false;
        }

        outImage.channels = (req_comp == 0) ? c : 4;

        STLW::Vector<float>& vec = outImage.pixels.emplace<STLW::Vector<float>>();
        vec.assign( data, data + ( w * h * outImage.channels ) );

        stbi_image_free( data );
    } 
    else
    {
        outImage.isHDR = false;
        unsigned char* data = stbi_load( filepath.c_str(), &w, &h, &c, req_comp );

        if ( !data ) {
            AXION_LOG_ERROR( Logger::Module::Core, "Failed to load image: {}", filepath );
            return false;
        }

        outImage.channels = (req_comp == 0) ? c : 4;

        STLW::Vector<unsigned char>& vec = outImage.pixels.emplace<STLW::Vector<unsigned char>>();
        vec.assign( data, data + ( w * h * outImage.channels ) );

        stbi_image_free( data );
    }

    // Default 2D Size
    outImage.size = {
        .width  = static_cast<u32>( w ),
        .height = static_cast<u32>( h ),
        .depth  = 1,
    };

    // 3D Texture Logic (Vertical Strip assumed: Width = TileSize, Height = TileSize * Depth)
    if ( flags & TextureImportAs3DTexture ) 
    {
       
        u32 tileSize = static_cast<u32>( w );
        u32 totalHeight = static_cast<u32>( h );
        
        if ( tileSize > 0 && (totalHeight % tileSize == 0) ) {
            outImage.size = {
                .width  = tileSize,
                .height = tileSize,
                .depth  = totalHeight / tileSize, 
            };
        } else {
             AXION_LOG_WARN( Logger::Module::Core, "Texture import as 3D failed dimensions check (Not a vertical strip?). Importing as 2D." );
        }
    }
    
    outImage.precision = outImage.isHDR ? TexturePrecision::F32 : TexturePrecision::U8;

    return true;
}

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END

//