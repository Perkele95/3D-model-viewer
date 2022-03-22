#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2D environmentMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSpherical(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan + 0.5;
    return uv;
}

void main()
{
    const vec3 uv = SampleSpherical(normalize(inPosition));
    const vec3 colour = texture(environmentMap, uv).rgb;
    outColour = vec4(colour, 1.0);
}