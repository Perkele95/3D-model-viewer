#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform samplerCube cubeMapSampler;

void main()
{
    vec3 colour = texture(cubeMapSampler, inUVW, 0.0).rgb;

    // Reinhard HDR tonemapping
    colour = colour / (colour + vec3(1.0));

    // Gamma correct
    colour = pow(colour, vec3(0.4545));

    outColour = vec4(colour, 1.0);
}