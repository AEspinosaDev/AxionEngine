#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IResource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( ISwapchain, Swapchain )

/**
 * @brief Interface representing a generic rendering swapchain.
 *
 * A swapchain manages the presentation of rendered images to a window surface.
 * It encapsulates multiple backbuffers,
 * and image acquisition logic.
 *
 * @note Concrete implementations exist per backend (e.g., DX12, Vulkan).
 */
class ISwapchain : public IObject
{
public:
    /**
     * @brief Swapchain creation parameters.
     */
    struct Description {
        Extent2D    size;                         ///< Dimensions of the swapchain images.
        Format      format = Format::RGBA8_UNORM; ///< Color format of the swapchain images.
        uint        imageCount;                   ///< Number of backbuffers/images.
        PresentMode presentMode;                  ///< Presentation mode (e.g., immediate, vsync, mailbox, etc.).
        bool        tearingSupported = false;     ///< Whether tearing is supported on this platform.
        std::string debugName        = "Swapchain";
    };

    virtual ~ISwapchain() = default;

    /**
     * @brief Present the current image to the screen.
     */
    virtual void present() = 0;

    /**
     * @brief Recreate or refresh swapchain images (e.g., after window resize).
     */
    virtual void updateImages() = 0;

    /**
     * @brief Acquire the index of the next image for rendering.
     *
     * @return Index of the next available swapchain image.
     */
    virtual uint acquireNextImage() = 0;

    /**
     * @brief Get the index of the current backbuffer image.
     *
     * @return Current image index.
     */

    virtual uint getCurrentImageIndex() = 0;

    /**
     * @brief Retrieve the swapchain description used during creation.
     *
     * @return Const reference to the swapchain description.
     */
    virtual const Description& getDescription() = 0;

    // virtual void change_format( Image_Format format ) = 0; ///< Optional: change the swapchain image format.

    /**
     * @brief Retrieve the swapchain images used for swaping.
     *
     * @return Const reference to the vector that contains them. Careful ! References of the images can be modified.
     */
    virtual const std::vector<TexturePtr>& getSwapImages() = 0;

    /**
     * @brief Reconfigures the swapchain with new description.
     *
     *
     */
    virtual void update( const Description& newDesc ) = 0;
};

typedef ISwapchain::Description SwapchainDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END
