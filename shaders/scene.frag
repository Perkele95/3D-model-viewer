#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColour;

layout(location = 0) out vec4 outColour;

void main()
{
    const vec3 normal = normalize(fragNormal);
    const vec3 cameraPos = vec3(0.8, 0.8, -2.0);
    const vec3 lightPosition = vec3(-1.8, 0.8, 2.0);
    const vec3 lightColour = vec3(1.0);

    const float ambientStrength = 0.01;
    const vec3 ambientLight = ambientStrength * lightColour;

    const vec3 lightDirection = normalize(lightPosition - fragPosition);
    const float diffValue = max(dot(normal, lightDirection), 0.0);
    const vec3 diffuse = lightColour * diffValue;

    const float specularStrength = 0.5;
    const vec3 viewDirection = normalize(cameraPos - fragPosition);
    const vec3 reflectDirection = reflect(-lightDirection, normal);
    const float specularFactor = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);
    const vec3 specular = specularStrength * specularFactor * lightColour;

    const vec3 result = ambientLight + diffuse + specular;
    outColour = fragColour * vec4(result, 1.0);
}