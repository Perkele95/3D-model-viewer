#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColour;
layout(location = 3) in vec3 fragCameraPosition;

layout(location = 0) out vec4 outColour;

layout(push_constant) uniform material3D
{
    vec4 ambient;
    vec4 diffuse;
    vec3 specular;
    float shininess;
} material;

void main()
{
    const vec3 normal = normalize(fragNormal);
    const vec3 lightPosition = vec3(3.0, -3.0, -3.0);
    const vec3 lightColour = vec3(1.0);

    const vec3 ambientLight = lightColour * vec3(material.ambient);

    const vec3 lightDirection = normalize(lightPosition - fragPosition);
    const float diffuseFactor = max(dot(normal, lightDirection), 0.0);
    const vec3 diffuse = lightColour * (vec3(material.diffuse) * diffuseFactor);

    const vec3 viewDirection = normalize(fragCameraPosition - fragPosition);
    const vec3 reflectDirection = reflect(-lightDirection, normal);
    const float specularFactor = pow(max(dot(viewDirection, reflectDirection), 0.0), 128.0 * material.shininess);
    const vec3 specular = lightColour * (vec3(material.specular) * specularFactor);

    const vec3 result = ambientLight + diffuse + specular;
    outColour = fragColour * vec4(result, 1.0);
}