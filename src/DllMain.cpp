#include <Windows.h>
#include <iostream>
#include <sstream>
#include "InjectedGameClass.h"
#include "tekken.h"
#include "utils.h"

void print(std::string output);
DWORD WINAPI MainThread(LPVOID param);
void bossesMain(uintptr_t baseAddress);
bool loadHeihachi(uintptr_t moveset, int bossCode);

InjectedGameClass Game;

using namespace Tekken;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(hModule);
    CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr); // Pass hModule to the thread
    break;
  case DLL_PROCESS_DETACH:
    // Cleanup if necessary
    break;
  }
  return TRUE;
}

DWORD WINAPI MainThread(LPVOID param)
{
  HMODULE hModule = (HMODULE)param;

  // Example interaction
  uintptr_t baseAddress = (uintptr_t)GetModuleHandle(NULL);
  
  std::stringstream ss;
  ss << "TEKKEN - Base Address: 0x" << std::hex << baseAddress << "\n";
  print(ss.str());
  // MessageBoxA(NULL, ss.str().c_str(), "Injection", MB_OK);

  // Do your operations here...
  bossesMain(baseAddress);

  // Unload the DLL by calling FreeLibraryAndExitThread
  FreeLibraryAndExitThread(hModule, 0);
  return 0;
}

void print(std::string output)
{
  OutputDebugStringA(output.c_str());
}

void bossesMain(uintptr_t baseAddress)
{
  Game.setBaseAddress(baseAddress);

  // As a test, I'll load boss Heihachi
  uintptr_t playerStructAddr = Game.getAddress({ 0x09ADA3B0, 0x30 });
  std::stringstream ss;
  ss << "TEKKEN - P1 Address: 0x" << std::hex << playerStructAddr << "\n";
  print(ss.str());
  ss.clear();
  uintptr_t moveset = Game.readUInt64(playerStructAddr + 0x2F48);
  ss << "TEKKEN - P1 Moveset: 0x" << std::hex << moveset << "\n";
  print(ss.str());
  ss.clear();
  if (moveset != 0)
  {
    loadHeihachi(moveset, 353);
  }
}

bool loadHeihachi(uintptr_t moveset, int bossCode)
{
  uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
  uintptr_t reqCount = Game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
  uintptr_t addr = 0;
  int req = 0, param = 0;
  int targetParam = bossCode - 350;
  for (uintptr_t i = 0; i < reqCount; i++)
  {
    addr = reqHeader + i * Sizes::Moveset::Requirement;
    req = Game.readInt32(addr);
    param = Game.readInt32(addr + 4);
    if ((req == 806 && param == targetParam) || req == 801 || (req == 802 && param >= 2049))
    {
      Game.write<int64_t>(addr, 0);
    }
  }

  // Game.writeString(moveset + 8, "ALI");
  print("TEKKEN - Heihachi loaded");
  return true;
}
