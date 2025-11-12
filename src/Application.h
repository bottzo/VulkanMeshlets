#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include "Module.h"
#include <vector>

class Module;

class Application final
{
public:
	Application();
	~Application();
	bool Init();
	UpdateStatus Update();
	bool CleanUp();
private:
	std::vector<Module*> modules;
	uint64_t lastTickMs = 0;
	uint64_t lastPerformanceCounter = 0;
	uint64_t performanceFrequency;
};

#endif // __APPLICATION_H__