#include "game.h"

// One-shot cleanup for leftover factory camera cave + hook patch.
// Build: g++ -o restore.exe restore.cpp && restore.exe
// Reads cave VA from abs-jmp target at hook+6 when patched (FF 25).

static constexpr size_t HOOK_PATCH_SIZE = 14;

static constexpr uintptr_t CAMERA_FACTORY_HOOK_RVA = 0x591BFF0;
static constexpr uint8_t CAMERA_FACTORY_HOOK_ORIGINAL[HOOK_PATCH_SIZE] = {
    0x40, 0x53,                   // push rbx
    0x48, 0x83, 0xEC, 0x30,       // sub rsp, 30
    0x48, 0x8B, 0xD9,             // mov rbx, rcx
    0xB9, 0x60, 0x01, 0x00, 0x00}; // mov ecx, 0x160

static void restoreHook(GameClass &game, const char *name, uintptr_t rva, const uint8_t *original)
{
  uintptr_t hookAddr = game.getBaseAddress() + rva;
  uint8_t current[HOOK_PATCH_SIZE] = {};
  if (!game.readBytes(hookAddr, current, sizeof(current)))
  {
    printf("%s: failed to read hook site 0x%llX\n", name, (unsigned long long)hookAddr);
    return;
  }

  if (current[0] != 0xFF || current[1] != 0x25)
  {
    printf("%s: not patched (first bytes %02X %02X); leaving alone\n",
           name, current[0], current[1]);
    return;
  }

  uintptr_t caveAddr = 0;
  memcpy(&caveAddr, current + 6, sizeof(caveAddr));

  DWORD oldProtect = 0;
  if (!game.protectMemory(hookAddr, HOOK_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect))
  {
    printf("%s: failed to unprotect hook site 0x%llX\n", name, (unsigned long long)hookAddr);
    return;
  }

  if (game.writeBytes(hookAddr, original, HOOK_PATCH_SIZE))
    printf("%s: restored original bytes at 0x%llX (rva 0x%llX)\n",
           name, (unsigned long long)hookAddr, (unsigned long long)rva);
  else
    printf("%s: failed to restore hook bytes at 0x%llX\n", name, (unsigned long long)hookAddr);

  game.protectMemory(hookAddr, HOOK_PATCH_SIZE, oldProtect, &oldProtect);

  if (caveAddr)
  {
    game.freeInTarget(reinterpret_cast<void *>(caveAddr));
    printf("%s: freed cave 0x%llX\n", name, (unsigned long long)caveAddr);
  }
}

int main()
{
  GameClass game;
  if (!game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    printf("Failed to attach to Tekken 8\n");
    return 1;
  }

  restoreHook(game, "Camera factory", CAMERA_FACTORY_HOOK_RVA, CAMERA_FACTORY_HOOK_ORIGINAL);
  return 0;
}
