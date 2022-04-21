#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec2 outUV;

void main()
{
    // passes (0, 0) - (1, 0) - (0, 1) - (1, 1) to fragment shader

	outUV = vec2((gl_VertexIndex << 1) & 1, gl_VertexIndex & 1);
	gl_Position = vec4(outUV, 0.0f, 1.0f);
}