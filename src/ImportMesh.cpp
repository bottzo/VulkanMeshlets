#include "ImportMesh.h"
#include "Globals.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

bool ImporterMesh::ImportFirst(const char* gltfPath, Mesh& outMesh)
{
	tinygltf::TinyGLTF gltfContext;
	tinygltf::Model model;

	std::string error, warning;
	bool loadOk = gltfContext.LoadASCIIFromFile(&model, &error, &warning, gltfPath);
	if (!loadOk)
	{
		LOG("[MODEL] Error loading gltf %s: %s", gltfPath, error.c_str());
		return false;
	}
	//for (const tinygltf::Mesh& mesh : model.meshes)
	//	for (const tinygltf::Primitive& primitive : mesh.primitives)
	//		Import(model, primitive, outMesh);

	if (model.meshes.empty() || model.meshes[0].primitives.empty())
	{
		LOG("The gltf does not contain a mesh to import");
		return false;
	}
	return Import(model, model.meshes[0].primitives[0], outMesh);
}

bool ImporterMesh::Import(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Mesh& mesh)
{
	const auto& itPos = primitive.attributes.find("POSITION");
	const auto& itNorm = primitive.attributes.find("NORMAL");

	if (itPos == primitive.attributes.end())
	{
		LOG("Error: The imported mesh does not have vertex positions");
		return false;
	}
	if (itNorm == primitive.attributes.end())
	{
		LOG("Error: The imported mesh does not have normals");
		return false;
	}
	if (primitive.indices == -1)
	{
		LOG("Error: The imported mesh does not have indices");
		return false;
	}

	const tinygltf::Accessor& posAcc = model.accessors[itPos->second];
	const tinygltf::Accessor& normAcc = model.accessors[itNorm->second];

	assert(posAcc.type == TINYGLTF_TYPE_VEC3);
	assert(posAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
	const tinygltf::BufferView& posView = model.bufferViews[posAcc.bufferView];
	const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
	const float* bufferPos = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAcc.byteOffset]);
	assert(normAcc.type == TINYGLTF_TYPE_VEC3);
	assert(normAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
	const tinygltf::BufferView& normView = model.bufferViews[normAcc.bufferView];
	const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];
	const float* bufferNorm = reinterpret_cast<const float*>(&normBuffer.data[normView.byteOffset + normAcc.byteOffset]);

	assert(posAcc.count == normAcc.count, "Error importing the mesh, the mesh does not have the same number of position and normal attributes");
	mesh.numVertices = posAcc.count;
	LOG("NumVertices: %u", mesh.numVertices);
	mesh.vertices = new Vertex[mesh.numVertices];

	for (unsigned int i = 0; i < mesh.numVertices; ++i)
	{
		Vertex& vertex = mesh.vertices[i];

		//position
		vertex.position[0] = bufferPos[0];
		vertex.position[1] = bufferPos[1];
		vertex.position[2] = bufferPos[2];
		if (posView.byteStride != 0) {
			bufferPos = reinterpret_cast<const float*>(reinterpret_cast<const char*>(bufferPos) + posView.byteStride);
		}
		else {
			bufferPos += 3;
		}

		//normal
		vertex.normal[0] = bufferNorm[0];
		vertex.normal[1] = bufferNorm[1];
		vertex.normal[2] = bufferNorm[2];
		if (normView.byteStride != 0)
		{
			bufferNorm = reinterpret_cast<const float*>(reinterpret_cast<const char*>(bufferNorm) + normView.byteStride);
		}
		else
		{
			bufferNorm += 3;
		}
	}

	//Indices part
	const tinygltf::Accessor& indAcc = model.accessors[primitive.indices];
	mesh.numIndices = indAcc.count;
	LOG("Num Indices: %u", mesh.numIndices);
	LOG("Num Triangles: %u", mesh.numIndices / 3);
	const tinygltf::BufferView& indView = model.bufferViews[indAcc.bufferView];
	const unsigned char* buffer = &(model.buffers[indView.buffer].data[indAcc.byteOffset + indView.byteOffset]);

	mesh.indices = new unsigned int[mesh.numIndices];

	switch (indAcc.componentType)
	{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			const uint32_t* bufferInd = reinterpret_cast<const uint32_t*>(buffer);
			for (unsigned int i = 0; i < mesh.numIndices; ++i)
			{
				mesh.indices[i] = bufferInd[i];
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			const uint16_t* bufferInd = reinterpret_cast<const uint16_t*>(buffer);
			for (unsigned int i = 0; i < mesh.numIndices; ++i)
			{
				mesh.indices[i] = bufferInd[i];
			}
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			const uint8_t* bufferInd = reinterpret_cast<const uint8_t*>(buffer);
			for (unsigned int i = 0; i < mesh.numIndices; ++i)
			{
				mesh.indices[i] = bufferInd[i];
			}
			break;
		}
	}
	return true;
}