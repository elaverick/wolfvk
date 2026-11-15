#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_colour;
layout(location = 0) out vec3 vertex_colour;

layout(push_constant) uniform ViewProjection {
    mat4 matrix;
} view_projection;

void main()
{
    gl_Position = view_projection.matrix * vec4(in_position, 1.0);
    vertex_colour = in_colour;
}
