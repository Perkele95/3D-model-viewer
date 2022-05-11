#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2D hdrTexture;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    const vec2 uv = SampleSphericalMap(normalize(inUVW));
    const vec3 colour = texture(hdrTexture, uv).rgb;
    outColour = vec4(colour, 1.0);
}