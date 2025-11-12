#ifndef __MODULE_H__
#define __MODULE_H__

#include "Globals.h"

class Module
{
public:
	virtual ~Module() {}
	virtual bool Init() { return true; }
	virtual UpdateStatus PreUpdate(float dt) { return UpdateStatus::UPDATE_CONTINUE; }
	virtual UpdateStatus Update(float dt) { return UpdateStatus::UPDATE_CONTINUE; }
	virtual UpdateStatus PostUpdate(float dt) { return UpdateStatus::UPDATE_CONTINUE; }
	virtual bool CleanUp() { return true; }
};

#endif // __MODULE_H__