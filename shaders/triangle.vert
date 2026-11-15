#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_colour;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in uint in_texture_layer;
layout(location = 4) in uint in_use_texture;

layout(location = 0) out vec3 vertex_colour;
layout(location = 1) out vec2 texture_coordinate;
layout(location = 2) flat out uint texture_layer;
layout(location = 3) flat out uint use_texture;

layout(push_constant) uniform ViewProjection {
    mat4 matrix;
} view_projection;

void main()
{
    gl_Position = view_projection.matrix * vec4(in_position, 1.0);
    vertex_colour = in_colour;
    texture_coordinate = in_texcoord;
    texture_layer = in_texture_layer;
    use_texture = in_use_texture;
}
