#version 330 core

uniform sampler2D uImage;
uniform sampler2D uCage;       // RGBA32F, rg = Verschiebung in Pixeln
uniform ivec2 uGridSize;       // n, m
uniform vec2 uImageSize;       // width, height

in vec2 vUv;
out vec4 fragColor;

vec2 cageDisplacement(vec2 uv)
{
    vec2 g = clamp(uv, 0.0, 1.0) * vec2(uGridSize - ivec2(1));

    ivec2 p0 = ivec2(floor(g));
    ivec2 p1 = min(p0 + ivec2(1), uGridSize - ivec2(1));

    vec2 f = fract(g);

    vec2 d00 = texelFetch(uCage, p0, 0).rg;
    vec2 d10 = texelFetch(uCage, ivec2(p1.x, p0.y), 0).rg;
    vec2 d01 = texelFetch(uCage, ivec2(p0.x, p1.y), 0).rg;
    vec2 d11 = texelFetch(uCage, p1, 0).rg;

    vec2 dx0 = mix(d00, d10, f.x);
    vec2 dx1 = mix(d01, d11, f.x);

    return mix(dx0, dx1, f.y);
}

void main()
{
    vec2 targetUv = vUv;

    // Inverse Mapping per Fixpunktiteration.
    // Gesucht: source + D(source) = target
    vec2 sourceUv = targetUv;

    for (int i = 0; i < 6; ++i) {
        vec2 d = cageDisplacement(sourceUv) / uImageSize;
        sourceUv = targetUv - d;
    }

    if (sourceUv.x < 0.0 || sourceUv.y < 0.0 ||
        sourceUv.x > 1.0 || sourceUv.y > 1.0) {
        fragColor = vec4(0.0);
    } else {
        fragColor = texture(uImage, sourceUv);
    }
}
