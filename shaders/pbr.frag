#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform camera_data
{
    mat4 view;
    mat4 proj;
    vec4 position;
} camera;

layout(binding = 1) uniform light_data
{
    vec4 positions[4];
    vec4 colours[4];
} lights;

layout(binding = 2) uniform sampler2D albedoMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D roughnessMap;
layout(binding = 5) uniform sampler2D metallicMap;
layout(binding = 6) uniform sampler2D ambientMap;

const float PI = 3.14159265359;

struct pbr_material
{
    vec3 albedo;
    vec3 normal;
    float roughness;
    float metallic;
    float ao;
};

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
    const vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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

vec3 BRDF(pbr_material material, vec3 V, vec3 lightPosition, vec3 lightColour, vec3 position)
{
    const vec3 L = normalize(lightPosition - position);
    const vec3 H = normalize(V + L); // halfway vector
    const float dotNV = max(dot(material.normal, V), 0.0);
    const float dotNL = max(dot(material.normal, L), 0.0);
    const float dotLH = max(dot(L, H), 0.0);
    const float dotNH = max(dot(material.normal, H), 0.0);
    const float dotHV = max(dot(H, V), 0.0);

    const float dist = length(lightPosition - position);
    const float attenuation = 1.0 / (dist * dist);
    const vec3 radiance = lightColour * attenuation;

    const float D = DistributionGGX(dotNH, material.roughness);
    const vec3 F = FresnelSchlick(dotHV, material.albedo, material.metallic);
    const float G = GeometrySchlickSmithGGX(dotNL, dotNV, material.roughness);

    const vec3 Ks = F;
    const vec3 Kd = (vec3(1.0) - Ks) * (1.0 - material.metallic);

    const vec3 numerator = D * F * G;
    const float denominator = 4.0 * dotNV * dotNL + 0.0001; // + avoids dividing by zero
    const vec3 specular = numerator / denominator;

    return (Kd * material.albedo / PI + specular) * radiance * dotNL;
}

vec3 calculateNormal()
{
    const vec3 tangentNormal = texture(normalMap, inUV).rgb * 2.0 - 1.0;

    const vec3 Q1 = dFdx(inPosition);
    const vec3 Q2 = dFdy(inPosition);
    const vec2 st1 = dFdx(inUV);
    const vec2 st2 = dFdy(inUV);

    const vec3 N = normalize(inNormal);
    const vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    const vec3 B = -normalize(cross(N, T));

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

void main()
{
    pbr_material material;
    material.albedo = pow(texture(albedoMap, inUV).rgb, vec3(2.2));
    //material.normal = normalize(inNormal);
    material.normal = calculateNormal();
    material.roughness = texture(roughnessMap, inUV).r;
    material.metallic = texture(metallicMap, inUV).r;
    material.ao = texture(ambientMap, inUV).r;

	const vec3 V = normalize(camera.position.xyz - inPosition);

    // Specular contribution
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lights.positions.length(); i++){
        const vec3 lp = lights.positions[i].xyz;
        const vec3 lc = lights.colours[i].xyz;
        Lo += BRDF(material, V, lp, lc, inPosition);
    }

    // Ambient combine
    const vec3 ambient = vec3(0.02) * material.albedo * material.ao;
    vec3 colour = ambient + Lo;

    // HDR tonemapping
    colour = colour / (colour + vec3(1.0));

    // Gamma correct
    colour = pow(colour, vec3(0.4545));

    outColour = vec4(colour, 1.0);
}