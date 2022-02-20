#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

layout(binding = 0) uniform camera_data
{
    mat4 view;
    mat4 proj;
    vec4 position;
} camera;

layout(push_constant) uniform object_data
{
    layout(offset = 0) mat4 model;
} object;

void main()
{
    outPosition = vec3(object.model * vec4(inPosition, 1.0));
    outNormal = mat3(object.model) * inNormal;
    outUV = inUV;
    gl_Position = camera.proj * camera.view * vec4(outPosition, 1.0);
}