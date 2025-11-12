//#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

#define LOG_BUFF_SIZE 4096

void log(const char file[], int line, const char* format, ...)
{
	static char tmpString[LOG_BUFF_SIZE];
	static char tmpString2[LOG_BUFF_SIZE];
	static va_list  ap;

	// Construct the string from variable arguments
	va_start(ap, format);
	vsprintf_s(tmpString, LOG_BUFF_SIZE, format, ap);
	va_end(ap);
	sprintf_s(tmpString2, LOG_BUFF_SIZE, "\n%s(%d) : %s", file, line, tmpString);
	int a = 3;
	std::cout << a << std::endl;
	std::cout << tmpString2;

#ifdef _WIN32
	OutputDebugString(tmpString2);
#endif // _WIN32
}