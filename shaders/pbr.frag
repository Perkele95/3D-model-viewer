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
    float exposure;
    float gamma;
} lights;

layout(binding = 2) uniform sampler2D brdfLUT;
layout(binding = 3) uniform samplerCube irradianceMap;
layout(binding = 4) uniform samplerCube prefilteredMap;

layout(binding = 5) uniform sampler2D albedoMap;
layout(binding = 6) uniform sampler2D normalMap;
layout(binding = 7) uniform sampler2D roughnessMap;
layout(binding = 8) uniform sampler2D metallicMap;
layout(binding = 9) uniform sampler2D ambientMap;

const float PI = 3.14159265359;

// Distribution

float DistributionGGX(float dotNH, float roughness)
{
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float denom = dotNH * dotNH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / (PI * denom * denom);
}

// Fresnel

vec3 FresnelSchlick(vec3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Geometry

float GeometrySchlickSmithGGX(float dotNL, float dotNV, float roughness)
{
    const float r = roughness + 1.0;
    const float k = (r * r) / 8.0;
    const float kInv = 1.0 - k;

    const float GL = dotNL / (dotNL * kInv + k);
    const float GV = dotNV / (dotNV * kInv + k);
    return GL * GV;
}

vec3 SpecularColour(vec3 L, vec3 V, vec3 N, vec3 F0, vec3 albedo,
                    vec3 radiance, float roughness, float metallic)
{
    const vec3 H = normalize(V + L);
    const float dotNH = max(dot(N, H), 0.0);
    const float dotNV = max(dot(N, V), 0.0);
    const float dotNL = max(dot(N, L), 0.0);

    vec3 colour = vec3(0.0);

    if(dotNL > 0.0)
    {
        const float D = DistributionGGX(dotNH, roughness);
        const vec3 F = FresnelSchlick(F0, dotNV);
        const float G = GeometrySchlickSmithGGX(dotNL, dotNV, roughness);
        const vec3 specular = D * F * G / (4 * dotNL * dotNV + 0.001);
        const vec3 kD = (1.0 - F) * (1.0 - metallic);
        colour += (kD * albedo / PI + specular) * radiance * dotNL;
    }

    return colour;
}

vec3 PrefilteredReflection(vec3 R, float roughness)
{
    const float MAX_REFLECTION_LOD = 8.0;
    const float lod = roughness * MAX_REFLECTION_LOD;
    return texture(prefilteredMap, R, lod).rgb;
}

vec3 SrgbToLinear(vec3 source)
{
    return pow(source, vec3(2.2));
}

vec3 CalculateNormal()
{
    const vec3 tangentNormal = SrgbToLinear(texture(normalMap, inUV).rgb);

    const vec3 Q1 = dFdx(inPosition);
    const vec3 Q2 = dFdy(inPosition);
    const vec2 st1 = dFdx(inUV);
    const vec2 st2 = dFdy(inUV);

    const vec3 N = normalize(inNormal);
    const vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    const vec3 B = normalize(cross(N, T));

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 colour)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((colour*(A*colour+C*B)+D*E)/(colour*(A*colour+B)+D*F))-E/F;
}

void main()
{
    const vec3 albedo = SrgbToLinear(texture(albedoMap, inUV).rgb);
    const float roughness = texture(roughnessMap, inUV).r;
    const float metallic = texture(metallicMap, inUV).r;

    const vec3 N = CalculateNormal();
	const vec3 V = normalize(camera.position.xyz - inPosition);
    const vec3 R = reflect(-V, N);
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lights.positions.length(); i++)
    {
        const vec3 L = normalize(lights.positions[i].xyz - inPosition);
        const float dist = length(lights.positions[i].xyz - inPosition);
        const vec3 radiance = lights.colours[i].xyz * (1.0 / dist * dist);

        Lo += SpecularColour(L, V, N, F0, albedo, radiance, roughness, metallic);
    }

	const vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    const vec3 irradiance = texture(irradianceMap, N).rgb;
    const vec3 reflection = PrefilteredReflection(R, roughness).rgb;

    const vec3 diffuse = irradiance * albedo;

    const vec3 F = FresnelSchlickRoughness(F0, max(dot(N, V), 0.0), roughness);
    const vec3 specular = reflection * (F * brdf.x + brdf.y);

    const vec3 kD = (1.0 - F) * (1.0 - metallic);
    const vec3 ambient = (kD * diffuse + specular) * texture(ambientMap, inUV).rrr;

    vec3 colour = ambient + Lo;

    // Tonemapping
    colour = Uncharted2Tonemap(colour * lights.exposure);

    // Gamma correct
    colour = pow(colour, vec3(1.0 / lights.gamma));

    outColour = vec4(colour, 1.0);
}