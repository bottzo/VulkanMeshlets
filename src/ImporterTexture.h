#ifndef __IMPORTER_TEXTURE_H__
#define __IMPORTER_TEXTURE_H__

namespace Importer
{
	namespace Texture
	{
		bool Import(const char* path, VkImage image, VkDeviceMemory memory);
	}
}

#endif // !__IMPORTER_MATERIAL_H__