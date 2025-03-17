#include "game.h"
#include "tekken.h"
#include <chrono>

int main()
{
  GameClass game;
  if (game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    auto start = std::chrono::high_resolution_clock::now(); // Start timer
    uintptr_t addr = 0;
    uintptr_t startAddr = game.getBaseAddress();

    // Player Struct Base Address
    addr = game.FastAoBScan(Tekken::PLAYER_STRUCT_SIG_BYTES, startAddr + 0x5A00000);
    // addr = addr + 7 + game.readUInt32(addr + 3);
    startAddr = addr;
    addr -= game.getBaseAddress();
    printf("Player Struct Base Address: 0x%llX\n", addr);

    // Match Struct Base Address
    addr = game.FastAoBScan(Tekken::MATCH_STRUCT_SIG_BYTES, startAddr);
    // addr = addr + 7 + game.readUInt32(addr + 3);
    addr -= game.getBaseAddress();
    printf("Match Struct Base Address: 0x%llX\n", addr);

    // Encryption Method
    addr = game.FastAoBScan(Tekken::ENC_SIG_BYTES, game.getBaseAddress() + 0x1700000);
    addr -= game.getBaseAddress();
    printf("Encryption Method Address: 0x%llX\n", addr);

    // HUD Icon
    addr = game.FastAoBScan(Tekken::HUD_ICON_SIG_BYTES, startAddr);
    addr += 13;
    addr -= game.getBaseAddress();
    printf("HUD Icon Address: 0x%llX\n", addr);

    // HUD Name
    addr = game.FastAoBScan(Tekken::HUD_NAME_SIG_BYTES, addr + 0x10, addr + 0x1000);
    addr += 13;
    addr -= game.getBaseAddress();
    printf("HUD Name Address: 0x%llX\n", addr);

    // Moveset Offset
    addr = game.FastAoBScan(Tekken::MOVSET_OFFSET_SIG_BYTES, game.getBaseAddress() + 0x1800000);
    // addr = game.readInt32(addr + 3);
    addr -= game.getBaseAddress();
    printf("Moveset Offset Address: 0x%llX\n", addr);

    // Devil Flag Offset
    addr = game.FastAoBScan(Tekken::DEVIL_FLAG_SIG_BYTES, game.getBaseAddress() + 0x2C00000);
    // addr = game.readInt32(addr + 3);
    addr -= game.getBaseAddress();
    printf("Devil Flag Offset Address: 0x%llX\n", addr);

    auto end = std::chrono::high_resolution_clock::now(); // End timer

    // Calculate elapsed time in milliseconds
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Time taken: " << elapsed.count() / 1000.0 << " s" << std::endl;
  }
  return 0;
}
