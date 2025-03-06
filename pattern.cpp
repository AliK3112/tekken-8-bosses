#include "game.h"
#include "tekken.h"
#include <chrono>

int main()
{
  GameClass game;
  if (game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    auto start = std::chrono::high_resolution_clock::now(); // Start timer

    // uintptr_t startAddr = game.getBaseAddress() + 0x5A0000;
    // uintptr_t endAddr = game.getBaseAddress() + 0x5FFFFFF;
    uintptr_t addr = game.FastAoBScan(Tekken::DEVIL_FLAG_SIG_BYTES);
    addr = game.readInt32(addr + 3);

    auto end = std::chrono::high_resolution_clock::now(); // End timer

    // Calculate elapsed time in milliseconds
    std::chrono::duration<double, std::milli> elapsed = end - start;

    printf("Address: 0x%llx\n", addr);
    std::cout << "Time taken: " << elapsed.count() << " ms" << std::endl;
  }
  return 0;
}
