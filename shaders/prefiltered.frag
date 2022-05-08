#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColour;

layout(binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform matrix
{
    layout(offset = 128) float roughness;
} values;

const uint NUM_SAMPLES = 1024;//TODO(arle): push constant
const float PI = 3.14159265359;

// http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(vec2 co)
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt = dot(co.xy, vec2(a,b));
	float sn = mod(dt, 3.14);
	return fract(sin(sn) * c);
}

// Hammersley Points on the Hemisphere by Holger Dammertz
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 Hammersley(uint i, uint N)
{
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    const float rdi = float(bits) * 2.3283064365386963e-10;
    return vec2(float(i) / float(N), rdi);
}

vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 N)
{
    const float a = roughness * roughness;

    const float phi = 2 * PI * Xi.x + random(N.xz) * 0.1;
    const float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    const vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    const vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    const vec3 tangentX = normalize(cross(up, N));
    const vec3 tangentY = cross(N, tangentX);

    // Convert from tangent to world space
    return tangentX * H.x + tangentY * H.y + N * H.z;
}

float DistributionGGX(float dotNH, float roughness)
{
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float denom = dotNH * dotNH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / (PI * denom * denom);
}

vec3 prefilter(vec3 R, float roughness)
{
    const vec3 N = R;
    const vec3 V = R;

    float weight = 0.0;
    vec3 colour = vec3(0.0);

    for(uint i = 0; i < NUM_SAMPLES; i++)
    {
        const vec2 Xi = Hammersley(i, NUM_SAMPLES);
        const vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        const vec3 L = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            colour += texture(environmentMap, inUVW).rgb * NdotL;
            weight += NdotL;
        }
    }
    return colour / weight;
}

void main()
{
    const vec3 N = normalize(inUVW);
    outColour = vec4(prefilter(N, values.roughness), 1.0);
}