#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColour;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform camera_data
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 position;
} camera;

layout(binding = 1) uniform light_data
{
    vec4 positions[4];
    vec4 colours[4];
} lights;

layout(push_constant) uniform pbr_material
{
    vec3 albedo;
    float roughness;
    float metallic;
    float ambientOcclusion;
} material;

const float PI = 3.14159265359;

void main()
{
    outColour = fragColour;
}