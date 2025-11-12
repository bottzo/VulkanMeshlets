#ifndef __MODULE_INPUT_H__
#define __MODULE_INPUT_H__

#include "Module.h"
#include "Globals.h"
#include "SDL3/SDL_scancode.h"

enum class KeyState : unsigned char
{
	KEY_IDLE,
	KEY_UP,
	KEY_DOWN,
	KEY_REPEAT
};

enum class MouseKey : unsigned char
{
	BUTTON_LEFT,
	BUTTON_MIDDLE,
	BUTTON_RIGHT,
	BUTTON_X1,
	BUTTON_X2,

	NUM_MOUSE_BUTTONS
};

class ModuleInput final : public Module
{
public:
	ModuleInput();
	~ModuleInput();

	bool Init() override;
	UpdateStatus PreUpdate(float dt) override;
	bool CleanUp() override;
	KeyState GetKey(SDL_Scancode scancode) { return keyboard[scancode]; }
	KeyState GetMouseKey(MouseKey key) { return mouseButtons[static_cast<unsigned char>(key)]; }
	bool MouseMotion() { return mouseMotion; }
	bool GetMouseMotion(float& x, float& y) { x = mouseMotionX; y = mouseMotionY; return mouseMotion; }

private:
	KeyState keyboard[SDL_SCANCODE_COUNT];
	KeyState mouseButtons[static_cast<unsigned char>(MouseKey::NUM_MOUSE_BUTTONS)];
	float mouseX;
	float mouseY;
	bool mouseMotion = false;
	float mouseMotionX;
	float mouseMotionY;

};

#endif // !__MODULE_INPUT_H__