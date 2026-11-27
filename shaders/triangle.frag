#version 450

layout(set = 0, binding = 0) uniform sampler2DArray wall_textures;

layout(location = 0) in vec3 vertex_colour;
layout(location = 1) in vec2 texture_coordinate;
layout(location = 2) flat in uint texture_layer;
layout(location = 3) flat in uint use_texture;

layout(location = 0) out vec4 fragment_colour;

layout(push_constant) uniform DamageFlash {
    layout(offset = 64) float intensity;
} damage_flash;

void main()
{
    vec4 surface_colour = vec4(vertex_colour, 1.0);

    if (use_texture != 0u) {
        surface_colour *= texture(
            wall_textures,
            vec3(texture_coordinate, float(texture_layer)));
    }

    /* Blend toward Wolfenstein's red damage palette without hiding the scene. */
    surface_colour.rgb = mix(
        surface_colour.rgb, vec3(0.65, 0.02, 0.02),
        clamp(damage_flash.intensity, 0.0, 1.0) * 0.45);
    fragment_colour = surface_colour;
}
