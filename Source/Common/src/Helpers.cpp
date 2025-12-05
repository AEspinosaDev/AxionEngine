#include "Axion\Common\Helpers.h"
#include <stb_image.h>



AXION_NAMESPACE_BEGIN
namespace Helpers {

ImageData Helpers::loadImage( const std::string& path, int forceChannels, bool flipVertically ) {
    ImageData result;
    int       w, h, c;

    stbi_set_flip_vertically_on_load( flipVertically );
    if ( stbi_is_hdr( path.c_str() ) )
    {
        result.isHDR = true;
        float* data  = stbi_loadf( path.c_str(), &w, &h, &c, forceChannels );

        if ( !data )
            throw std::runtime_error( "Failed to load HDR image: " + path );

        result.width    = static_cast<uint>( w );
        result.height   = static_cast<uint>( h );
        result.channels = forceChannels > 0 ? forceChannels : c;

        std::vector<float>& vec = result.pixels.emplace<std::vector<float>>();
        vec.assign( data, data + ( w * h * result.channels ) );

        stbi_image_free( data );
    }
    else
    {
        result.isHDR = false;
        unsigned char* data = stbi_load( path.c_str(), &w, &h, &c, forceChannels );

        if ( !data )
            throw std::runtime_error( "Failed to load image: " + path );

        result.width    = static_cast<uint>( w );
        result.height   = static_cast<uint>( h );
        result.channels = forceChannels > 0 ? forceChannels : c;

        std::vector<unsigned char>& vec = result.pixels.emplace<std::vector<unsigned char>>();
        vec.assign( data, data + ( w * h * result.channels ) );

        stbi_image_free( data );
    }

    return result;
}

} // namespace Helpers

AXION_NAMESPACE_END