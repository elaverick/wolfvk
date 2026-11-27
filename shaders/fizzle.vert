#version 450

/*
 * The original FizzleFade reveals a new destination one pseudo-random pixel
 * at a time. A full-screen triangle lets the fragment shader reproduce that
 * transition without depending on scene geometry or texture coordinates.
 */
void main()
{
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
