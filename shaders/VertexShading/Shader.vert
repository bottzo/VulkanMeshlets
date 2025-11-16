#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;

layout(std140, binding = 4) uniform transformations
{
	mat4 viewProj;
    vec3 cameraPos;
};
layout(std430, binding = 5) readonly buffer Transforms 
{
	mat4 models[];
};

layout(location=0) out vec3 normal;
layout(location=1) out flat uint meshletID;
layout(location=2) out flat uint meshID;

//TODO
//meshletIn.modelID -> gl_DrawIndex
//meshletIn.meshletID -> gl_InstanceIndex

void main()
{
	const mat4 model = models[gl_DrawIndex];
	gl_Position = viewProj * model * vec4(pos, 1);
	normal = transpose(inverse(mat3(model))) * norm;
	meshletID = meshletIn.meshletID;
	meshID = gl_DrawIndex;	
}