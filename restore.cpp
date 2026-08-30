#include "game.h"

// One-shot cleanup for a leftover story-camera code cave + hook patch.
// Build: g++ -o restore.exe restore.cpp && restore.exe

static constexpr uintptr_t LEFTOVER_CAVE = 0xB5CC70000ull;
static constexpr uintptr_t CAMERA_HOOK_RVA = 0x5C38A40;
static constexpr size_t CAMERA_HOOK_PATCH_SIZE = 14;
static constexpr uint8_t CAMERA_HOOK_ORIGINAL[CAMERA_HOOK_PATCH_SIZE] = {
    0x48, 0x89, 0x5C, 0x24, 0x08, // mov [rsp+8], rbx
    0x48, 0x89, 0x74, 0x24, 0x18, // mov [rsp+18], rsi
    0x55,                         // push rbp
    0x57,                         // push rdi
    0x41, 0x54};                  // push r12

int main()
{
  GameClass game;
  if (!game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    printf("Failed to attach to Tekken 8\n");
    return 1;
  }

  uintptr_t hookAddr = game.getBaseAddress() + CAMERA_HOOK_RVA;
  uint8_t current[CAMERA_HOOK_PATCH_SIZE] = {};
  if (game.readBytes(hookAddr, current, sizeof(current)))
  {
    // Absolute jmp patch starts with FF 25
    if (current[0] == 0xFF && current[1] == 0x25)
    {
      DWORD oldProtect = 0;
      if (game.protectMemory(hookAddr, sizeof(CAMERA_HOOK_ORIGINAL), PAGE_EXECUTE_READWRITE, &oldProtect))
      {
        if (game.writeBytes(hookAddr, CAMERA_HOOK_ORIGINAL, sizeof(CAMERA_HOOK_ORIGINAL)))
          printf("Restored original bytes at hook 0x%llX (rva 0x%llX)\n",
                 (unsigned long long)hookAddr,
                 (unsigned long long)CAMERA_HOOK_RVA);
        else
          printf("Failed to restore hook bytes at 0x%llX\n", (unsigned long long)hookAddr);
        game.protectMemory(hookAddr, sizeof(CAMERA_HOOK_ORIGINAL), oldProtect, &oldProtect);
      }
      else
      {
        printf("Failed to unprotect hook site 0x%llX\n", (unsigned long long)hookAddr);
      }
    }
    else
    {
      printf("Hook site not patched (first bytes %02X %02X); leaving it alone\n",
             current[0], current[1]);
    }
  }
  else
  {
    printf("Failed to read hook site 0x%llX\n", (unsigned long long)hookAddr);
  }

  game.freeInTarget(reinterpret_cast<void *>(LEFTOVER_CAVE));
  printf("Freed 0x%llX\n", (unsigned long long)LEFTOVER_CAVE);
  return 0;
}
