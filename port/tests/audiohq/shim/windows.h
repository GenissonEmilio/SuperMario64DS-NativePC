// Linux stand-in for the three FindFile calls hal/sdat/sdat.cpp makes, so the
// mod layer and the mixer can be built and tested off Windows (host_audio_test).
#pragma once
#include <stdint.h>
typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
struct WIN32_FIND_DATAA {
    unsigned dwFileAttributes;
    unsigned nFileSizeHigh;
    unsigned nFileSizeLow;
    char cFileName[260];
};
HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *fd);
int FindNextFileA(HANDLE h, WIN32_FIND_DATAA *fd);
int FindClose(HANDLE h);
