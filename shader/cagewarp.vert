#version 330 core

out vec2 vUv;

void main()
{
    vec2 p[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 pos = p[gl_VertexID];
    gl_Position = vec4(pos, 0.0, 1.0);

    // Qt/QImage-Koordinaten: y nach unten
    vUv = vec2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
}
