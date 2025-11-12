#include "ModuleInput.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_events.h"
//#include "imgui.h"
//#include "imgui_impl_sdl3.h"
//#include "imgui_impl_opengl3.h"

ModuleInput::ModuleInput() : mouseX(0.0f), mouseY(0.0f)
{

}

ModuleInput::~ModuleInput()
{

}

bool ModuleInput::Init() 
{
	if (!SDL_Init(SDL_INIT_EVENTS))
		return false;
	return true;
}

UpdateStatus ModuleInput::PreUpdate(float dt) 
{
	mouseMotion = false;
	mouseMotionX = 0.0f;
	mouseMotionY = 0.0f;

	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
			case SDL_EVENT_QUIT:
				return UpdateStatus::UPDATE_STOP;
			case SDL_EVENT_MOUSE_MOTION:
				mouseMotion = true;
				mouseMotionX = ev.motion.xrel;
				mouseMotionY = ev.motion.yrel;
				break;
		}
	}

	int numKeys;
	const bool* keys = SDL_GetKeyboardState(&numKeys);
	for (int i = 0; i < numKeys; ++i)
	{
		if (keys[i])
		{
			if (keyboard[i] == KeyState::KEY_IDLE)
				keyboard[i] = KeyState::KEY_DOWN;
			else
				keyboard[i] = KeyState::KEY_REPEAT;
		}
		else
		{
			if (keyboard[i] == KeyState::KEY_REPEAT || keyboard[i] == KeyState::KEY_DOWN)
				keyboard[i] = KeyState::KEY_UP;
			else
				keyboard[i] = KeyState::KEY_IDLE;
		}
	}

	SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouseX, &mouseY);
	for (int i = 0; i <= static_cast<int>(MouseKey::NUM_MOUSE_BUTTONS); ++i)
	{
		if (buttons & SDL_BUTTON_MASK(i + 1))
		{
			if (mouseButtons[i] == KeyState::KEY_IDLE)
				mouseButtons[i] = KeyState::KEY_DOWN;
			else
				mouseButtons[i] = KeyState::KEY_REPEAT;
		}
		else
		{
			if (mouseButtons[i] == KeyState::KEY_REPEAT || mouseButtons[i] == KeyState::KEY_DOWN)
				mouseButtons[i] = KeyState::KEY_UP;
			else
				mouseButtons[i] = KeyState::KEY_IDLE;
		}
	}

	return UpdateStatus::UPDATE_CONTINUE;
}

bool ModuleInput::CleanUp()
{
	SDL_QuitSubSystem(SDL_INIT_EVENTS);
	return true;
}