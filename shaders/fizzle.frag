#version 450

layout(location = 0) out vec4 fragment_colour;

layout(push_constant) uniform DeathFizzle {
    layout(offset = 64) float progress;
    layout(offset = 68) uint pixel_size;
} death_fizzle;

/* A stable integer hash supplies the pseudo-random reveal order for each pixel block. */
uint fizzle_pixel_order(uvec2 pixel)
{
    uint value = pixel.x * 0x9E3779B9u ^ pixel.y * 0x85EBCA6Bu;

    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

void main()
{
    uint block_size = max(death_fizzle.pixel_size, 1u);
    uvec2 pixel = uvec2(gl_FragCoord.xy) / block_size;
    uint order = fizzle_pixel_order(pixel) >> 8;
    float reveal_threshold = float(order) * (1.0 / 16777216.0);

    if (reveal_threshold >= clamp(death_fizzle.progress, 0.0, 1.0)) {
        discard;
    }

    /* VGA palette entry 4 is the dark red used by the original Died() screen. */
    fragment_colour = vec4(0.4, 0.0, 0.0, 1.0);
}
