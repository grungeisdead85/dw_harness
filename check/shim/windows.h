// Minimal stand-in for <windows.h>, only to syntax-check the DLL source on Linux.
#pragma once
#include <cstdint>
#include <cstddef>
typedef unsigned long DWORD; typedef int BOOL; typedef void* PVOID; typedef void* LPVOID; typedef const void* LPCVOID;
typedef size_t SIZE_T; typedef void* HMODULE; typedef const wchar_t* LPCWSTR; typedef const char* LPCSTR; typedef DWORD* PDWORD;
typedef union { struct { DWORD LowPart; long HighPart; } u; long long QuadPart; } LARGE_INTEGER;
typedef struct { PVOID BaseAddress; PVOID AllocationBase; DWORD AllocationProtect; SIZE_T RegionSize; DWORD State; DWORD Protect; DWORD Type; } MEMORY_BASIC_INFORMATION;
#define MEM_COMMIT 0x1000
#define PAGE_NOACCESS 0x01
#define PAGE_READWRITE 0x04
#define PAGE_GUARD 0x100
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
SIZE_T VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T);
BOOL VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD);
HMODULE GetModuleHandleW(LPCWSTR);
BOOL QueryPerformanceCounter(LARGE_INTEGER*);
BOOL QueryPerformanceFrequency(LARGE_INTEGER*);
PVOID InterlockedExchangePointer(PVOID volatile*, PVOID);
DWORD GetLastError();
DWORD GetFileAttributesA(LPCSTR);
extern "C" int _putenv_s(const char*, const char*);

// (15Z) the real windows.h's min and max macros (no NOMINMAX), so code MSVC can't build fails here too.
// MSVC's own headers are safe from them; libstdc++'s aren't, so the ones the DLL uses come in first.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef NOMINMAX
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#endif
