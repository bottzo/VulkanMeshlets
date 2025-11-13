#include "Globals.h"
#include "Application.h"

int main(int argc, char* argv[])
{
	Application* app = new Application();
	UpdateStatus appStatus = UpdateStatus::UPDATE_ERROR;
	if (app->Init())
	{
		do
		{
			appStatus = app->Update();
		} while (appStatus == UpdateStatus::UPDATE_CONTINUE);
	}
	if (appStatus != UpdateStatus::UPDATE_ERROR)
		app->CleanUp();
	else
		LOG("App closing with errors :(");
	delete app;
	return 0;
}