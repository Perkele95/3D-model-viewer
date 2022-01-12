#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;

layout(binding = 0) uniform camera_data
{
    mat4 view;
    mat4 proj;
    vec4 position;
} camera;

layout(push_constant) uniform object_data
{
    layout(offset = 32) mat4 model;
} object;

void main()
{
    fragPosition = vec3(object.model * vec4(inPosition, 1.0));
    fragNormal = mat3(object.model) * inNormal;
    gl_Position = camera.proj * camera.view * vec4(fragPosition, 1.0);
}