#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;

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

vec3 FresnelSchlick(float cosTheta, vec3 albedo, float metallic)
{
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    return F;
}

float GeometrySchlickSmithGGX(float dotNL, float dotNV, float roughness)
{
    const float r = roughness + 1.0;
    const float k = (r * r) / 8.0;
    const float kInv = 1.0 - k;

    const float GL = dotNL / (dotNL * kInv + k);
    const float GV = dotNV / (dotNV * kInv + k);
    return GL * GV;
}

vec3 BRDF(vec3 N, vec3 V, vec3 lightPosition, vec3 lightColour, vec3 position,
          vec3 albedo, float metallic, float roughness)
{
    const vec3 L = normalize(lightPosition - position);
    const vec3 H = normalize(V + L); // halfway vector
    const float dotNV = max(dot(N, V), 0.0);
    const float dotNL = max(dot(N, L), 0.0);
    const float dotLH = max(dot(L, H), 0.0);
    const float dotNH = max(dot(N, H), 0.0);
    const float dotHV = max(dot(H, V), 0.0);

    vec3 colour = vec3(0.0);

    const float dist = length(lightPosition - position);
    const float attenuation = 1.0 / (dist * dist);
    const vec3 radiance = lightColour * attenuation;

    const float D = DistributionGGX(dotNH, roughness);
    const vec3 F = FresnelSchlick(dotHV, albedo, metallic);
    const float G = GeometrySchlickSmithGGX(dotNL, dotNV, roughness);

    const vec3 Ks = F;
    const vec3 Kd = (vec3(1.0) - Ks) * (1.0 - metallic);

    const vec3 numerator = D * F * G;
    const float denominator = 4.0 * dotNV * dotNL + 0.0001; // + avoids dividing by zero
    const vec3 specular = numerator / denominator;

    return (Kd * albedo / PI + specular) * radiance * dotNL;
}

void main()
{
	const vec3 N = normalize(fragNormal);
	const vec3 V = normalize(camera.position.xyz - fragPosition);
    const vec3 albedo = material.albedo;

    // Specular contribution
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lights.positions.length(); i++){
        const vec3 lp = lights.positions[i].xyz;
        const vec3 lc = lights.colours[i].xyz;
        const float r = material.roughness;
        const float m = material.metallic;
        Lo += BRDF(N, V, lp, lc, fragPosition, albedo, m, r);
    }

    // Ambient combine
    const vec3 ambient = vec3(0.03) * albedo * material.ambientOcclusion;
    vec3 colour = ambient + Lo;

    // HDR tonemapping
    colour = colour / (colour + vec3(1.0));

    // Gamma correct
    colour = pow(colour, vec3(0.4545));

    outColour = vec4(colour, 1.0);
}