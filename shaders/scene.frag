#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColour;

layout(location = 0) out vec4 outColour;

vec4 calculateLight(vec4 colour, vec3 normal, vec3 lightDirection, vec3 lightColour)
{
    const float ambientStrength = 0.1;
    const vec4 ambientLight = vec4(ambientStrength * lightColour, 1.0);

    const float diffValue = max(dot(normal, lightDirection), 0.0);
    const vec4 diffuse = vec4(lightColour, 1.0) * diffValue;

    vec4 result = colour * (ambientLight + diffuse);
    return result;
}

void main()
{
    const vec3 lightColour = vec3(1.0);
    const vec3 lightPosition = vec3(1.0, 3.0, 1.0);
    const vec3 lightDirection = normalize(lightPosition - vec3(fragPosition));

    outColour = calculateLight(fragColour, fragNormal, lightDirection, lightColour);
}