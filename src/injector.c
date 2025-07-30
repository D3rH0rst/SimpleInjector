#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>
#include <TlHelp32.h>
#include "injector.h"

#define TARGET_EXECUTABLE_NAME TEXT("Spotify.exe")
#define TARGET_WNDCLASS_NAME   TEXT("Chrome_WidgetWin_1")
#define TARGET_DLL_PATH        TEXT(".\\SpotLink.dll")
#define TARGET_DLL_NAME        TEXT("SpotLink.dll")

#ifdef UNICODE
#define TARGETFUNC "LoadLibraryW"
#else
#define TARGETFUNC "LoadLibraryA"
#endif

DWORD GetTargetPID(TCHAR *);
DWORD IsDLLAlreadyLoaded(DWORD targetPID, TCHAR *);
BOOL FileExists(LPCTSTR szPath);
BOOL CheckTargetWindow(DWORD dwOwnerPID);

//int main(int argc, TCHAR **argv) {
//
//	if (argc != 3) {
//		_tprintf(TEXT("Usage: %s <dll_path> <process_name>"), argv[0]);
//		return 1;
//	}
//
//	TCHAR *target_dll_path = argv[1];
//	TCHAR *target_process_name = argv[2];
//
//	if (!FileExists(target_dll_path)) {
//		_tprintf(TEXT("Could not find target dll `%s`\n"), target_dll_path);
//		return 1;
//	}
//	
//	TCHAR full_dll_path[MAX_PATH];
//	DWORD dll_path_length = GetFullPathName(target_dll_path, sizeof(full_dll_path) / sizeof(*full_dll_path), full_dll_path, NULL);
//	
//	if (dll_path_length == 0) {
//		_tprintf(TEXT("Failed to get full dll path for `%s`\n"), target_dll_path);
//		return 1;
//	}
//	
//	_tprintf(TEXT("Target DLL `%s` full path: %s length: %ld\n"), target_dll_path, full_dll_path, dll_path_length);
//
//	DWORD target_pid = GetTargetPID(target_process_name);
//
//	if (!target_pid) {
//		_tprintf(TEXT("Failed to retrieve target pid\n"));
//		return 1;
//	}
//
//	_tprintf(TEXT("target pid (%s): %ld\n"), target_process_name, target_pid);
//
//	if (IsDLLAlreadyLoaded(target_pid, target_dll_path)) {
//		_tprintf(TEXT("`%s` is already present in %s\n"), target_dll_path, target_process_name);
//		return 1;
//	}
//
//	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pid);
//	if (!hProcess) {
//		_tprintf(TEXT("Failed to OpenProcess on %ld: %ld\n"), target_pid, GetLastError());
//		return 1;
//	}
//
//	PVOID pRemoteString = VirtualAllocEx(hProcess, NULL, dll_path_length + 1, MEM_COMMIT, PAGE_READWRITE);
//	if (!pRemoteString) {
//		_tprintf(TEXT("Failed to allocate memory in the target process\n"));
//		CloseHandle(hProcess);
//		return 1;
//	}
//	_tprintf(TEXT("Allocated memory for the dll string at 0x%p...\n"), pRemoteString);
//
//	if (!WriteProcessMemory(hProcess, pRemoteString, full_dll_path, dll_path_length + 1, NULL)) {
//		_tprintf(TEXT("Failed to write target process memory\n"));
//		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
//		CloseHandle(hProcess);
//		return 1;
//	}
//	_tprintf(TEXT("Wrote the dll string into target memory...\n"));
//	HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
//	if (!hKernel32) {
//		_tprintf(TEXT("Failed to get kernel32.dll handle\n"));
//		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
//		CloseHandle(hProcess);
//		return 1;
//	}
//	PVOID pLoadLibrary = GetProcAddress(hKernel32, TARGETFUNC);
//	if (!pLoadLibrary) {
//		_tprintf(TEXT("Failed to GetProcAddress of `") TEXT(TARGETFUNC) TEXT("` in ´kernel32.dll`\n"));
//		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
//		CloseHandle(hProcess);
//		return 1;
//	}
//	_tprintf(TEXT("Retrieved address for ") TEXT(TARGETFUNC) TEXT(" at 0x%p...\n"), pLoadLibrary);
//
//	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (PTHREAD_START_ROUTINE)pLoadLibrary, pRemoteString, 0, NULL);
//	if (!hThread) {
//		_tprintf(TEXT("Failed to CreateRemoteThread\n"));
//		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
//		CloseHandle(hProcess);
//		return 1;
//	}
//	else {
//		_tprintf(TEXT("Created remote thread for ") TEXT(TARGETFUNC) TEXT(" in target process...\n"));
//		WaitForSingleObject(hThread, 4000);
//		ResumeThread(hProcess);
//		CloseHandle(hThread);
//	}
//
//
//	_tprintf(TEXT("Successfully injected `%s` into `%s`\n"), full_dll_path, target_process_name);
//	VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
//	CloseHandle(hProcess);
//
//	return 0;
//}

int Inject(DWORD pid, TCHAR *dllPath) {
	
	log_msg("pid: %d, path: %s", pid, dllPath);

	int dllPathLength = _tcslen(dllPath);

	if (!FileExists(dllPath)) {
		MessageBox(NULL, TEXT("Could not find target DLL"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		return 1;
	}

	if (IsDLLAlreadyLoaded(pid, dllPath)) {
		MessageBox(NULL, TEXT("Target DLL already present in target process"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		return 1;
	}

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!hProcess) {
		MessageBox(NULL, TEXT("Failed to OpenProcess"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		return 1;
	}

	PVOID pRemoteString = VirtualAllocEx(hProcess, NULL, (dllPathLength + 1) * sizeof(TCHAR), MEM_COMMIT, PAGE_READWRITE);
	if (!pRemoteString) {
		MessageBox(NULL, TEXT("Failed to VitualAllocEx"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		CloseHandle(hProcess);
		return 1;
	}

	if (!WriteProcessMemory(hProcess, pRemoteString, dllPath, (dllPathLength + 1) * sizeof(TCHAR), NULL)) {
		MessageBox(NULL, TEXT("Failed to WriteProcessMemory"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return 1;
	}

	HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
	if (!hKernel32) {
		MessageBox(NULL, TEXT("Failed to get kernel32.dll handle"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return 1;
	}
	PVOID pLoadLibrary = GetProcAddress(hKernel32, TARGETFUNC);
	if (!pLoadLibrary) {
		MessageBox(NULL, TEXT("Failed to GetProcAddress of `") TEXT(TARGETFUNC) TEXT("` in `kernel32.dll`\n"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return 1;
	}

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (PTHREAD_START_ROUTINE)pLoadLibrary, pRemoteString, 0, NULL);
	if (!hThread) {
		MessageBox(NULL, TEXT("Failed to CreateRemoteThread"), TEXT("ERROR"), MB_ICONERROR | MB_OK);
		VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return 1;
	}

	WaitForSingleObject(hThread, 4000);
	CloseHandle(hThread);
	VirtualFreeEx(hProcess, pRemoteString, 0, MEM_RELEASE);
	CloseHandle(hProcess);

	_tprintf(TEXT("injected\n"));

	return 0;
}

BOOL FileExists(LPCTSTR szPath) {
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

DWORD GetTargetPID(TCHAR *executable_name) {
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	DWORD targetPID = 0;

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		_tprintf(TEXT("Failed to create process snapshot handle\n"));
		return 0;
	}

	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnap, &pe32)) {
		_tprintf(TEXT("Failed to get process with Process32First\n"));
		CloseHandle(hProcessSnap);
		return 0;
	}

	do {
		if (_tcscmp(pe32.szExeFile, executable_name) == 0) {
			CloseHandle(hProcessSnap);
			return pe32.th32ProcessID;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	if (targetPID == 0) {
		_tprintf(TEXT("Could not find any executable `%s`\n"), executable_name);
	}
	return targetPID;
}

DWORD IsDLLAlreadyLoaded(DWORD targetPID, TCHAR *dll_path) {

	HANDLE hModuleSnap;
	MODULEENTRY32 me32;

	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, targetPID);

	if (hModuleSnap == INVALID_HANDLE_VALUE) {
		_tprintf(TEXT("Failed to create process snapshot handle\n"));
		return 0;
	}

	me32.dwSize = sizeof(MODULEENTRY32);
	
	if (!Module32First(hModuleSnap, &me32)) {
		_tprintf(TEXT("Failed to get module with Module32First\n"));
		CloseHandle(hModuleSnap);
		return 0;
	}

	TCHAR *dll_name = _tcsrchr(dll_path, '\\') + 1;
	if (dll_name == 1) dll_name = dll_path;

	do {
		if (_tcscmp(me32.szModule, dll_name) == 0) {
			return TRUE;
		}
	} while (Module32Next(hModuleSnap, &me32));

	CloseHandle(hModuleSnap);
	
	return FALSE;
}

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam) {
	TCHAR class_name[100];
	GetClassName(hwnd, class_name, sizeof(class_name) / sizeof(*class_name));
	if (_tcscmp(class_name, TARGET_WNDCLASS_NAME) == 0) {
		*(BOOL*)lParam = TRUE;
		return FALSE;
	}
	return TRUE;
}

BOOL CheckTargetWindow(DWORD dwOwnerPID) {
	HANDLE hThreadSnap;
	THREADENTRY32 te32;

	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE) {
		_tprintf(TEXT("Failed to create thread snapshot handle\n"));
		return FALSE;
	}

	te32.dwSize = sizeof(THREADENTRY32);

	if (!Thread32First(hThreadSnap, &te32)) {
		_tprintf(TEXT("Failed to get Thread with Thread32First\n"));
		CloseHandle(hThreadSnap);
		return FALSE;
	}

	BOOL found = FALSE;
	do {
		if (te32.th32OwnerProcessID == dwOwnerPID) {
			EnumThreadWindows(te32.th32ThreadID, EnumThreadWndProc, (LPARAM)&found);
			if (found) break;
		}
	} while (Thread32Next(hThreadSnap, &te32));

	CloseHandle(hThreadSnap);

	return found;
}


