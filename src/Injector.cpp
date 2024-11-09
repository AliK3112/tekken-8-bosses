#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>

DWORD GetProcessIdByName(const wchar_t *processName)
{
  DWORD processId = 0;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot != INVALID_HANDLE_VALUE)
  {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
      do
      {
        if (wcscmp(entry.szExeFile, processName) == 0)
        {
          processId = entry.th32ProcessID;
          break;
        }
      } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
  }
  return processId;
}

bool InjectDLL(DWORD processId, const char *dllPath)
{
  HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
  if (!process)
    return false;

  // Allocate memory for the DLL path in the target process
  void *allocMem = VirtualAllocEx(process, nullptr, strlen(dllPath) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  WriteProcessMemory(process, allocMem, dllPath, strlen(dllPath) + 1, nullptr);

  // Get address of LoadLibraryA and create a remote thread to load the DLL
  HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
  FARPROC loadLibAddr = GetProcAddress(kernel32, "LoadLibraryA");
  HANDLE thread = CreateRemoteThread(process, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibAddr, allocMem, 0, nullptr);

  // Wait for the DLL to be loaded and free memory
  WaitForSingleObject(thread, INFINITE);
  VirtualFreeEx(process, allocMem, 0, MEM_RELEASE);
  CloseHandle(thread);
  CloseHandle(process);
  return true;
}

int main()
{
  const wchar_t *processName = L"Polaris-Win64-Shipping.exe";

  char exePath[MAX_PATH];
  if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0)
  {
    std::cerr << "Failed to get executable path." << std::endl;
    return 1;
  }
  // Remove the executable name to get the directory
  std::string exeDir = exePath;
  exeDir = exeDir.substr(0, exeDir.find_last_of("\\/") + 1);

  // Append the DLL file name to create the absolute DLL path
  std::string dllPath = exeDir + "libbosses.dll";

  // std::cout << "DLL PATH: " << dllPath << std::endl;
  DWORD processId = GetProcessIdByName(processName);
  if (processId)
  {
    if (InjectDLL(processId, dllPath.c_str()))
    {
      std::cout << "DLL Injected Successfully!" << std::endl;
    }
    else
    {
      std::cerr << "DLL Injection Failed!" << std::endl;
    }
  }
  else
  {
    std::cerr << "Target process not found!" << std::endl;
  }
  return 0;
}
