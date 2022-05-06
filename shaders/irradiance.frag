#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColour;

layout(binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main()
{
    const vec3 normal = normalize(inUVW);

    vec3 irradiance = vec3(0.0);

    const vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), normal));
    const vec3 forward = normalize(cross(normal, right));

    const float sampleDelta = 0.025;
    uint nSamples = 0;

    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            const float sinTheta = sin(theta);
            const float cosTheta = cos(theta);
            const vec3 tangentSample = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

            const vec3 vector = tangentSample.x * right + tangentSample.y * forward + tangentSample.z * normal;

            irradiance += texture(environmentMap, vector).rgb * cosTheta * sinTheta;
            nSamples++;
        }
    }

    outColour = vec4(irradiance * PI * (1.0 / float(nSamples)), 1.0);
}