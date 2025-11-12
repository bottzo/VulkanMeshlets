#include "FileSystem.h"
#include <stdio.h>
#include "Globals.h"

long FileSystem::ReadToBuffer(const char* path, char*& buffer, const char* mode)
{
    FILE* fileHandle = fopen(path, mode);
    if (fileHandle == NULL)
        return 0;
    fseek(fileHandle, 0, SEEK_END);
    long fileSize = ftell(fileHandle);
    if (fileSize != 0)
    {
        rewind(fileHandle);
        buffer = new char[fileSize];
        if (fread(buffer, sizeof(char), fileSize, fileHandle) < fileSize)
        {
            LOG("Error reading file %s", path);
        }
    }
    fclose(fileHandle);
    return fileSize;
}
