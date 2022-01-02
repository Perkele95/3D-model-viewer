#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColour;
layout(location = 3) in vec3 cameraPosition;

layout(location = 0) out vec4 outColour;

layout(binding = 1) uniform light_data
{
    vec4 positions[4];
    vec4 colours[4];
} lights;

layout(push_constant) uniform pbr_material
{
    layout(offset = 0) vec3 albedo;
    layout(offset = 12) float roughness;
    layout(offset = 16) float metallic;
    layout(offset = 20) float ambientOcclusion;
} material;

const float PI = 3.14159265359;

float DistributionGGX(float dotNH, float roughness)
{
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float denom = dotNH * dotNH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / (PI * denom * denom);
}

vec3 FresnelSchlick(float cosTheta, float metallic)
{
    const vec3 F0 = mix(vec3(0.04), material.albedo, metallic);
    const vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return F;
}

float GeometrySchlickSmithGGX(float dotNL, float dotNV, float roughness)
{
    const float r = roughness + 1.0;
    const float k = (r * r) / 8.0; // direct k
    const float kInv = 1.0 - k;
    const float GL = dotNL / (dotNL * kInv + k);
    const float GV = dotNV / (dotNV * kInv + k);
    return GV * GL;
}

vec3 BRDF(vec3 N, vec3 V, vec3 L, float metallic, float roughness)
{
    const vec3 H = normalize(V + L); // halfway vector
    const float dotNV = clamp(dot(N, V), 0.0, 1.0);
    const float dotNL = clamp(dot(N, L), 0.0, 1.0);
    const float dotLH = clamp(dot(L, H), 0.0, 1.0);
    const float dotNH = clamp(dot(N, H), 0.0, 1.0);

    const vec3 lightColour = vec3(1.0);

    vec3 colour = vec3(0.0);

    if(dotNL > 0.0){
        const float rroughness = max(0.05, roughness);// try without this line

        const float D = DistributionGGX(dotNH, roughness);
        const vec3 F = FresnelSchlick(dotNV, metallic);
        const float G = GeometrySchlickSmithGGX(dotNL, dotNV, roughness);

        const vec3 specular = D * F * G / (4.0 * dotNL * dotNV); // check if dotNL cancels out
        colour += specular * dotNL * lightColour;
    }

    return colour;
}

void main()
{
    const vec3 N = normalize(fragNormal);
    const vec3 V = normalize(cameraPosition - fragPosition);

    // Specular contribution
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lights.positions.length(); i++){
        const vec3 L = normalize(lights.positions[i].xyz - fragPosition);
        Lo += BRDF(N, V, L, material.metallic, material.roughness);
    }

    // Combine with ambient
    vec3 colour = (material.albedo * 0.02) + Lo;

    // Gamma correction
    colour = pow(colour, vec3(0.4545));

    //outColour = fragColour;
    outColour = vec4(colour, 1.0);
}