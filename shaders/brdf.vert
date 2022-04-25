#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec2 outUV;

void main()
{
	// indices 0, 1, 2, 3 give the following result for outUV:
	// (0, 0) - (1, 0) - (0, 1) - (1, 1)

	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
	outUV.y = 1.0 - outUV.y; // flip y around
}