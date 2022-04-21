#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;

const uint NUM_SAMPLES = 1024;
const float PI = 3.1415926536;

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
vec2 Hammersley(uint i, uint N) {
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

    const float phi = 2 * PI * Xi.x;// + random(normal.xz) * 0.1;
    const float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    const vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    const vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    const vec3 tangentX = normalize(cross(up, N));
    const vec3 tangentY = cross(N, tangentX);

    // Convert from tangent to world space
    return tangentX * H.x + tangentY * H.y + N * H.z;
}

float GeometrySchlickSmithGGX(float dotNL, float dotNV, float roughness)
{
    // k for IBL
    const float k = (roughness * roughness) / 2.0;
    const float GL = dotNL / (dotNL * (1.0 - k) + k);
    const float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

vec2 IntegrateBRDF(float roughness, float NoV)
{
	// Normal always points along z-axis for the 2D lookup
	const vec3 N = vec3(0.0, 0.0, 1.0);

    vec3 V;
    V.x = sqrt(1.0 - NoV * NoV); // som
    V.y = 0.0;
    V.z = NoV; // cos

    float A = 0.0;
    float B = 0.0;

    for(uint i = 0; i < NUM_SAMPLES; i++)
    {
        const vec2 Xi = Hammersley(i, NUM_SAMPLES);
        const vec3 H = ImportanceSampleGGX(Xi, roughness, N);
        const vec3 L = 2.0 * dot(V, H) * H - V;

        const float NoL = max(L.z, 0.0);
        const float NoH = max(H.z, 0.0);
        const float VoH = max(dot(V, H), 0.0);

        if(NoL > 0.0)
        {
            const float G = GeometrySchlickSmithGGX(NoL, NoV, roughness);
            const float GVis = G * VoH / (NoH * NoV);
            const float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * GVis;
            B += Fc * GVis;
        }
    }
    return vec2(A, B) / float(NUM_SAMPLES);
}

void main()
{
    outColour = vec4(IntegrateBRDF(inUV.s, inUV.t), 0.0, 1.0);
}