#include <Windows.h>
#include <iostream>

DWORD WINAPI MainThread(LPVOID param);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
    // Code to run when the DLL is injected
    DisableThreadLibraryCalls(hModule);                                                // Optional: reduce overhead
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr); // Start your main function in a new thread
    break;
  case DLL_PROCESS_DETACH:
    // Cleanup if necessary
    break;
  }
  return TRUE;
}

DWORD WINAPI MainThread(LPVOID param)
{
  // Example interaction
  MessageBoxA(NULL, "DLL Injected Successfully!", "Injection", MB_OK);
  // Your GameClass or other function calls go here
  return 0;
}
