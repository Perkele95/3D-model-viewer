#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform samplerCube cubeMapSampler;

layout(binding = 1) uniform light_data
{
    vec4 positions[4];
    vec4 colours[4];
    float exposure;
    float gamma;
} lights;

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
    vec3 colour = texture(cubeMapSampler, inUVW, 0.0).rgb;

    // Tonemapping
    colour = Uncharted2Tonemap(colour * lights.exposure);

    // Gamma correct
    colour = pow(colour, vec3(1.0 / lights.gamma));

    outColour = vec4(colour, 1.0);
}