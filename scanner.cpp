#include "game.h"
#include "tekken.h"

int main()
{
  GameClass game;

  if (!game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    printf("Failed to attach to Tekken 8\n");
    return 1;
  }

  const uintptr_t base = game.getBaseAddress();

  uintptr_t motbinOffset = 0;
  uintptr_t devilOffset = 0;
  uintptr_t playerOffset = 0;
  uintptr_t matchOffset = 0;

  auto scan = [&](auto signature, uintptr_t offset)
  {
    return game.FastAoBScan(signature, base + offset);
  };

  if (auto addr = scan(Tekken::MOVSET_OFFSET_SIG_BYTES, 0x1700000))
  {
    motbinOffset = game.readUInt32(addr + 3);
    printf("moveset_offset_addr_offset=0x%llX\n", addr - base);
  }

  if (auto addr = scan(Tekken::DEVIL_FLAG_SIG_BYTES, 0x1900000))
  {
    devilOffset = game.readUInt32(addr + 2);
    printf("permanent_devil_offset_addr_offset=0x%llX\n", addr - base);
  }

  if (auto addr = scan(Tekken::STORY_CAMERA_HOOK_SIG_BYTES, 0x5C00000))
  {
    printf("camera_hook_offset=0x%llX\n", addr - base);
  }

  if (auto addr = scan(Tekken::PLAYER_STRUCT_SIG_BYTES, 0x5A00000))
  {
    playerOffset = addr + 7 + game.readUInt32(addr + 3) - base;
    printf("player_struct_base_addr_offset=0x%llX\n", addr - base);
  }

  if (auto addr = scan(Tekken::HUD_ICON_SIG_BYTES, 0x5C00000))
  {
    addr += 13;
    printf("hud_icon_addr_offset=0x%llX\n", addr - base);
  }

  if (auto addr = scan(Tekken::HUD_NAME_SIG_BYTES, 0x5C00000))
  {
    addr += 13;
    printf("hud_name_addr_offset=0x%llX\n", addr - base);
  }

  if (auto addr = scan(Tekken::MATCH_STRUCT_SIG_BYTES, 0x5C00000))
  {
    matchOffset = addr + 7 + game.readUInt32(addr + 3) - base;
    printf("match_struct_base_addr_offset=0x%llX\n", addr - base);
  }

  printf("player_struct_base=0x%llX\n", playerOffset);
  printf("match_struct_base=0x%llX\n", matchOffset);
  printf("moveset_offset=0x%llX\n", motbinOffset);
  printf("permanent_devil_offset=0x%llX\n", devilOffset);

  return 0;
}
