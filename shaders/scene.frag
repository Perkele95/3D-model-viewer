#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColour;

layout(binding = 0) uniform camera_data
{
    vec4 position;
} camera;

layout(location = 0) out vec4 outColour;

struct material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

void main()
{
    material matr;
    matr.ambient = vec3(0.135, 0.2225, 0.1575);
    matr.diffuse = vec3(0.54, 0.89, 0.63);
    matr.specular = vec3(0.316228, 0.316228, 0.316228);
    matr.shininess = 0.1;

    const vec3 normal = normalize(fragNormal);
    const vec3 lightPosition = vec3(3.0, 3.0, -3.0);
    const vec3 lightColour = vec3(1.0);

    const vec3 ambientLight = lightColour * matr.ambient;

    const vec3 lightDirection = normalize(lightPosition - fragPosition);
    const float diffuseFactor = max(dot(normal, lightDirection), 0.0);
    const vec3 diffuse = lightColour * (matr.diffuse * diffuseFactor);

    const vec3 viewDirection = normalize(vec3(camera.position) - fragPosition);
    const vec3 reflectDirection = reflect(-lightDirection, normal);
    const float specularFactor = pow(max(dot(viewDirection, reflectDirection), 0.0), 128.0 * matr.shininess);
    const vec3 specular = lightColour * (matr.specular * specularFactor);

    const vec3 result = ambientLight + diffuse + specular;
    outColour = fragColour * vec4(result, 1.0);
}