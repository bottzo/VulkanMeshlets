#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 texCoord;


layout(binding = 0) uniform uboData 
{
	vec4 frustumPlanes[6];
	uint numModels;
};
layout(std140, binding = 1) uniform transformations
{
	mat4 viewProj;
    vec3 cameraPos;
};
layout(std430, binding = 2) readonly buffer Transforms 
{
	mat4 models[];
};
layout(std430, binding = 3) readonly buffer ModelIDs { uint modelIDs[]; };

layout(location=0) out vec3 normal;
layout(location=1) out flat uint meshletID;
layout(location=2) out flat uint meshID;
layout(location=3) out vec2 textureCoordinates;

//meshletID -> gl_DrawID
//currentProcessingMeshlet -> gl_InstanceIndex
//gl_VertexIndex -> index al curr Vertex

void main()
{
	const uint modelID = modelIDs[gl_DrawID * numModels + gl_InstanceIndex];
	const mat4 model = models[modelID];
	gl_Position = viewProj * model * vec4(pos, 1);
	normal = transpose(inverse(mat3(model))) * norm;
	textureCoordinates = texCoord;
	meshletID = gl_DrawID;
	meshID = modelID;	
}