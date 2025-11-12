#ifndef __IMPORTER_MESH_H__
#define __IMPORTER_MESH_H__

#include "ModuleVulkan.h"

namespace tinygltf {
	struct Model;
	struct Primitive;
}

namespace ImporterMesh
{
	bool ImportFirst(const char* gltfPath, Mesh& mesh);
	bool Import(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Mesh& mesh);
}

#endif // !__IMPORTER_MESH_H__
