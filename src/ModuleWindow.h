#ifndef __MODULE_WINDOW_H__
#define __MODULE_WINDOW_H__

#include "Module.h"

struct SDL_Window;

class ModuleWindow final : public Module
{
public:

	ModuleWindow();
	~ModuleWindow();

	bool Init() override;
	bool CleanUp() override;
	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return heigth; }
	SDL_Window* window;
private:
	unsigned int width;
	unsigned int heigth;
};

#endif // !__MODULE_WINDOW_H__