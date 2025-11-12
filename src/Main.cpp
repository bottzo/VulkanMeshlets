#include "Globals.h"
#include "Application.h"

int main(int argc, char* argv[])
{
	Application* app = new Application();
	if (app->Init())
		while (app->Update() == UpdateStatus::UPDATE_CONTINUE);
	app->CleanUp();
	delete app;
	return 0;
}