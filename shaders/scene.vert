#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform camera_data
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 position;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColour;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec4 fragColour;
layout(location = 3) out vec3 fragCameraPosition;

void main()
{
    fragPosition = vec3(camera.model * vec4(inPosition, 1.0));
    gl_Position = camera.proj * camera.view * camera.model * vec4(inPosition, 1.0);

    fragNormal = mat3(transpose(inverse(camera.model))) * inNormal;
    fragColour = inColour;
    fragCameraPosition = vec3(camera.position);
}