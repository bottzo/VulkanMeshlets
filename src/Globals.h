#ifndef __GLOBALS_H__
#define __GLOBALS_H__

enum class UpdateStatus : unsigned char
{
	UPDATE_STOP,
	UPDATE_CONTINUE,
	UPDATE_ERROR
};

//LOG
#define LOG(format, ...) log(__FILE__, __LINE__, format, __VA_ARGS__);
void log(const char file[], int line, const char* format, ...);

#endif // __GLOBALS_H__