#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;

layout(binding = 0) uniform camera_data
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 position;
} camera;

void main()
{
    const vec3 objectPosition = vec3(0.0);

    fragPosition = vec3(camera.model * vec4(inPosition, 1.0));
    fragNormal = mat3(camera.model) * inNormal;
    gl_Position = camera.proj * camera.view * vec4(fragPosition, 1.0);
}