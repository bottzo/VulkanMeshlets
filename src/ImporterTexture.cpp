#include "ImporterTexture.h"
#include "FileSystem.h"
#include "Globals.h"
#include "IL/il.h"
#include "ilu.h"
#include "vulkan/vulkan.h"

//Wrapper to init the devil library
class DevilLib
{
public:
	DevilLib() { ilInit(); iluInit(); }
	~DevilLib() { ilShutDown(); }
};

static constexpr unsigned int IlToVulkan(unsigned int glFormat)
{

}

bool Importer::Texture::Import(const char* path, ::Texture& texture)
{
	//Init the devil library
	static DevilLib lib;

	ILuint ilImg;
	ilGenImages(1, &ilImg);
	ilBindImage(ilImg);

	if (!ilLoadImage(path))
	{
		LOG("Error loading the image %s", path);
		for (ILenum lastError = ilGetError(); lastError != IL_NO_ERROR; lastError = ilGetError())
		{
			LOG("Devil reported error: %s", iluErrorString(lastError));
		}
		return false;
	}

	//ImportOptions
	iluFlipImage();

	//IL_IMAGE_FORMAT;
	//IL_IMAGE_WIDTH;
	//IL_IMAGE_HEIGHT;
	//IL_IMAGE_SIZE_OF_DATA;
	//IL_NUM_MIPMAPS;
	texture = ::Texture(ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), OpenGLToVulkan(ilGetInteger(IL_IMAGE_FORMAT)), IlTypeToGlType(ilGetInteger(IL_IMAGE_TYPE)), ilGetData(), ilGetInteger(IL_IMAGE_FORMAT));
	ilDeleteImages(1, &ilImg);

	return true;
}