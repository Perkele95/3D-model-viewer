#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform camera_matrix
{
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColour;

layout(location = 0) out vec4 fragColour;

void main()
{
    gl_Position = camera.proj * camera.view * camera.model * vec4(inPosition, 1.0);
    fragColour = inColour;
}