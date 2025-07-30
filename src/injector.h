#pragma once
#include <windows.h>
#include <tchar.h>

#define CONSOLE

#ifdef CONSOLE
#define log_msg(msg, ...) _tprintf(TEXT(msg) TEXT("\n"), __VA_ARGS__)
#else
#define log_msg(msg, ...)
#endif

int Inject(DWORD pid, TCHAR *dllPath);