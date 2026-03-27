#include "Axion\Common\Common.h"

AXION_NAMESPACE_BEGIN

Extent2D Extent3D::to2D() const {
    return { .width  = width,
             .height = height };
}
Extent3D Extent2D::to3D() const {
    return { .width  = width,
             .height = height,
             .depth  = 1 };
}
AXION_NAMESPACE_END