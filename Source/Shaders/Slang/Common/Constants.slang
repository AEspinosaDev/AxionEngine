// MODULE: engine-wide shader constants. import it.

public static const uint CANON_VERTEX_STRIDE = 64;

// Meshlets (must match Assets::Meshlet / MESHLET_MAX_* in Mesh.h)
public static const uint MESHLET_STRIDE        = 48;
public static const uint MESHLET_MAX_VERTICES  = 64;
public static const uint MESHLET_MAX_TRIANGLES = 126;

// Visibility buffer triangle id: ( meshlet index << VIS_TRIANGLE_BITS ) | meshlet local triangle
public static const uint VIS_TRIANGLE_BITS = 7; // 2^7 = 128 >= MESHLET_MAX_TRIANGLES
public static const uint VIS_TRIANGLE_MASK = ( 1u << VIS_TRIANGLE_BITS ) - 1u;

// PI
public static const float PI            = 3.141592653589793238462643;
public static const float PI_OVER_2     = PI / 2.0;
public static const float PI_OVER_4     = PI / 4.0;
public static const float INV_PI        = 1.0 / PI;
public static const float INV_PI_OVER_2 = 1.0 / PI_OVER_2;
public static const float INV_PI_OVER_4 = 1.0 / PI_OVER_4;
