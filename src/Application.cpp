#include "Application.h"
#include "Module.h"
#include "ModuleWindow.h"
#include "ModuleInput.h"
#include "ModuleVulkan.h"
#include "ModuleEditorCamera.h"
#include "SDL3/SDL_timer.h"

Application::Application() : performanceFrequency(SDL_GetPerformanceFrequency())
{
	//modules.reserve(); Alguna forma de fer saver quans modules hi haura?
	ModuleWindow* mWindow = new ModuleWindow();
	ModuleInput* mInput = new ModuleInput();
	ModuleEditorCamera* mCamera = new ModuleEditorCamera(mInput, mWindow);
	ModuleVulkan* mVulkan = new ModuleVulkan(mWindow, mCamera);
	modules.push_back(mWindow);
	modules.push_back(mInput);
	modules.push_back(mCamera);
	modules.push_back(mVulkan);
}

Application::~Application()
{
	for (std::vector<Module*>::reverse_iterator it = modules.rbegin(); it != modules.rend(); ++it)
	{
		delete *it;
	}
	modules.clear();
}

bool Application::Init()
{
	for (Module* mod : modules)
		if (!mod->Init())
			return false;

	performanceFrequency = SDL_GetPerformanceFrequency();
	return true;
}

UpdateStatus Application::Update()
{
	//const uint64_t currentTick = SDL_GetTicks();
	//const float dt = (currentTick - lastTickMs) / 1000.0f;
	//lastTickMs = currentTick;

	uint64_t counter = SDL_GetPerformanceCounter();
	uint64_t elapsed = counter - lastPerformanceCounter;
	lastPerformanceCounter = counter;
	float dt = static_cast<float>(elapsed) / static_cast<float>(performanceFrequency);
	UpdateStatus ret;
	for (Module* mod : modules)
	{
		ret = mod->PreUpdate(dt);
		if (ret != UpdateStatus::UPDATE_CONTINUE)
			return ret;
	}

	for (Module* mod : modules)
	{
		ret = mod->Update(dt);
		if (ret != UpdateStatus::UPDATE_CONTINUE)
			return ret;
	}

	for (Module* mod : modules)
	{
		ret = mod->PostUpdate(dt);
		if (ret != UpdateStatus::UPDATE_CONTINUE)
			return ret;
	}

	return UpdateStatus::UPDATE_CONTINUE;
}

bool Application::CleanUp()
{
	for (std::vector<Module*>::reverse_iterator it = modules.rbegin(); it != modules.rend(); ++it)
		if (!(*it)->CleanUp())
			return false;
	return true;
}