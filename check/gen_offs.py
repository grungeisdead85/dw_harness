# Makes offs.cpp from native.lua's OFF table: every offset checked against the DLL's struct Block.
# Usage: python3 gen_offs.py <native.lua> > offs.cpp
import sys, re
src = open(sys.argv[1]).read()
body = re.search(r'^local OFF = \{(.*?)^\}', src, re.S | re.M).group(1)
body = re.sub(r'--[^\n]*', '', body)
pairs = re.findall(r'(\w+)\s*=\s*(\d+)', body)
print('''#include <cstdio>
#include "windows.h"
SIZE_T VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T) { return 0; }
BOOL VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD) { return 1; }
HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
BOOL QueryPerformanceCounter(LARGE_INTEGER*) { return 1; }
BOOL QueryPerformanceFrequency(LARGE_INTEGER*) { return 1; }
PVOID InterlockedExchangePointer(PVOID volatile* p, PVOID v) { PVOID o = *p; *p = v; return o; }
DWORD GetLastError() { return 0; }
DWORD GetFileAttributesA(LPCSTR) { return 0; }
extern "C" int _putenv_s(const char*, const char*) { return 0; }
#include "../detailed_weather_dll.cpp"
int main() {
  int bad = 0;''')
for n, v in pairs:
    print('  if (offsetof(Block, %s) != %s) { printf("%s %%zu %s\\n", offsetof(Block, %s)); bad++; }' % (n, v, n, v, n))
print('  printf("%%d offsets checked, %%d differ\\n", %d, bad);\n  return bad;\n}' % len(pairs))
