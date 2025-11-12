#ifndef __FILE_SYSTEM_H__
#define __FILE_SYSTEM_H__

namespace FileSystem
{
	//TODO: crear una enum pel mode i fer una funció constexpr que ens doni el const char* a partir de la enum del mode (lookup)
	long ReadToBuffer(const char* path, char*& buffer, const char* mode);
}

#endif // !__FILE_SYSTEM_H__
