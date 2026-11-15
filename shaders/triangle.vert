#version 450

layout(location = 0) out vec3 vertex_colour;

const vec2 positions[3] = vec2[](
    vec2(0.0, -0.55),
    vec2(0.55, 0.45),
    vec2(-0.55, 0.45)
);

const vec3 colours[3] = vec3[](
    vec3(0.95, 0.28, 0.16),
    vec3(0.20, 0.75, 0.92),
    vec3(0.95, 0.78, 0.22)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.5, 1.0);
    vertex_colour = colours[gl_VertexIndex];
}
