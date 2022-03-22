#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outPosition;

layout(push_constant) uniform matrix
{
    mat4 proj;
    mat4 view;
} camera;

void main()
{
    outPosition = inPosition;
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
}