#include "ModuleWindow.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"

ModuleWindow::ModuleWindow() : window(nullptr), width(700), heigth(700)
{

}

ModuleWindow::~ModuleWindow()
{

}

bool ModuleWindow::Init()
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		LOG("Could not init SDL_VIDEO");
		return false;
	}

	//Opengl attributes required to know before we create the window
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); // desired version
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	//
	//SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // we want a double buffer (it is the default)
	//SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); // we want to have a depth buffer with minimum 24 bits
	//SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8); // we want to have a stencil buffer with minimum 8 bits

//#ifdef _DEBUG
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
//#endif // _DEBUG

	//Uint32 flags = SDL_WINDOW_OPENGL;
	Uint32 flags = SDL_WINDOW_VULKAN;

//window start params
//#define WINDOW_FULLSCREEN
#define WINDOW_RESIZEABLE

#ifdef WINDOW_FULLSCREEN
		flags |= SDL_WINDOW_FULLSCREEN;
#endif
#ifdef WINDOW_RESIZEABLE
		flags |= SDL_WINDOW_RESIZABLE;
#endif

	window = SDL_CreateWindow("Engine", width, heigth, flags);

	if (window == nullptr)
	{
		LOG("Could not create window, Error: %s", SDL_GetError());
		return false;
	}

	return true;
}

bool ModuleWindow::CleanUp()
{
	if (window != nullptr)
		SDL_DestroyWindow(window);
	SDL_Quit();
	return true;
}