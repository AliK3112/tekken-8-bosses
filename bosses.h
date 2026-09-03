// This class will be responsible for loading boss characters
#include "moveset.h"
#include "charcodes.h"
#include "utils.h"
#include <array>
#include <cstring>
#include <vector>

using namespace Tekken;

static constexpr const char *FINAL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_naked_belt_off.CS_ant_1p_naked_belt_off";
static constexpr const char *CHAINED_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_chain.CS_ant_1p_chain";
static constexpr const char *FINAL_KAZ_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_grl_1p_v2_white.CS_grl_1p_v2_white";
static constexpr const char *DEVIL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_swl_ant_1p.CS_swl_ant_1p";
static constexpr const char *DEVIL_JIN_COSTUME_PATH_2 = "/Game/Demo/Story/Sets/CS_swl_ant_1p_horn.CS_swl_ant_1p_horn";
static constexpr const char *DEVIL_JIN_COSTUME_PATH_3 = "/Game/Demo/Story/Sets/CS_swl_ant_1p_horn_bw.CS_swl_ant_1p_horn_bw";
static constexpr const char *HEIHACHI_MONK_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_whitetiger_nohat_nomask.CS_bee_whitetiger_nohat_nomask";
static constexpr const char *HEIHACHI_SHADOW_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_1p_p_shadow.CS_bee_1p_p_shadow";
bool INSTALL_CAMERA_HOOKS = true;

bool isCorrectCharacter(int bossCode, int charId);
bool isValidJinBoss(int bossCode);
bool isValidDevilJinBoss(int bossCode);
bool isValidKazuyaBoss(int bossCode);
bool isValidHeihachiBoss(int bossCode);
bool isCorrectHeihachiFlag(int storyFlag, int param);
bool isStoryCameraBoss(int bossCode);

struct ConfigFlags {
  bool disableAutoParries = false;
  bool handleHudAndCostumes = true;
  bool toneDownDamage = false;
  bool finalKazuyaRageBlast = true;
};

struct CameraTrainerState
{
  uint32_t p1BossCode; // +0x00
  uint32_t p2BossCode; // +0x04
  uint32_t eligible;   // +0x08
};

class TkBossLoader
{
private:
  // MEMBERS
  int bossCode_L = BossCodes::None;
  int bossCode_R = BossCodes::None;
  bool attached = false;
  bool ready = false;
  // ADDRESSES
  uintptr_t playerStructOffset = 0;
  uintptr_t matchStructOffset = 0;
  uintptr_t movesetOffset = 0;
  uintptr_t permaDevilOffset = 0;
  uintptr_t heihachiWIOffset = 0;
  uintptr_t decryptFuncAddr = 0;
  uintptr_t hudIconAddr = 0;
  uintptr_t hudNameAddr = 0;
  // jz rel8 at sig+13 — masked in AoB so already-NOP'd (0x90 0x90) sites still match
  static constexpr uint16_t HUD_ICON_ORIG = 0x5274; // 74 52
  static constexpr uint16_t HUD_NAME_ORIG = 0x3174; // 74 31
  static constexpr uint16_t HUD_PATCH_NOP = 0x9090;
  uintptr_t cameraHookAddr = 0;
  uintptr_t dramaCameraHookAddr = 0;
  // STORY CAMERA HOOK
  CameraTrainerState *cameraRemoteState = nullptr;
  uint8_t *cameraCodeCave = nullptr;
  bool cameraHookInstalled = false;
  // Absolute jmp qword ptr [rip+0] + imm64 = 14 bytes (through push r12)
  static constexpr size_t CAMERA_HOOK_PATCH_SIZE = 14;
  static constexpr uint32_t CAMERA_ID_STORY_DELTA = 0xDB;
  const uint8_t cameraHookOriginal[CAMERA_HOOK_PATCH_SIZE] = {
      0x48, 0x89, 0x5C, 0x24, 0x08, // mov [rsp+8], rbx
      0x48, 0x89, 0x74, 0x24, 0x18, // mov [rsp+18], rsi
      0x55,                         // push rbp
      0x57,                         // push rdi
      0x41, 0x54};                  // push r12
  // DRAMA CAMERA HOOK (Devil Jin intro/winpose: R9D 121 -> 12)
  uint8_t *dramaCameraCodeCave = nullptr;
  bool dramaCameraHookInstalled = false;
  static constexpr size_t DRAMA_CAMERA_HOOK_PATCH_SIZE = 14;
  const uint8_t dramaCameraHookOriginal[DRAMA_CAMERA_HOOK_PATCH_SIZE] = {
      0x48, 0x89, 0x5C, 0x24, 0x08, // mov [rsp+8], rbx
      0x55,                         // push rbp
      0x56,                         // push rsi
      0x57,                         // push rdi
      0x41, 0x54,                   // push r12
      0x41, 0x55,                   // push r13
      0x41, 0x56};                  // push r14
  // CONFIGURATIONS
  bool devMode = false;
  bool handleIcons = false;
  HWND hwndLogBox = nullptr;
  ConfigFlags* config = nullptr;

  bool shouldHandleHudAndCostumes() {
    return config ? config->handleHudAndCostumes : true;
  }

  bool shouldDisableAutoParries() {
      return config ? config->disableAutoParries : false;
  }

  bool shouldToneDownDamage() {
      return config ? config->toneDownDamage : false;
  }

  bool shouldGiveFinalKazuyaRageBlast() {
      return config ? config->finalKazuyaRageBlast : true;
  }

  // METHODS
  // Append message to log box (GUI) and/or stdout (console / devMode)
  void AppendLog(const std::string &msg)
  {
    if (msg.empty())
      return; // Prevent appending empty messages

    // Ensure no excessive newlines
    std::string trimmedMsg = msg;
    while (!trimmedMsg.empty() && (trimmedMsg.back() == '\n' || trimmedMsg.back() == '\r'))
    {
      trimmedMsg.pop_back();
    }

    if (hwndLogBox)
    {
      int length = GetWindowTextLengthA(hwndLogBox);
      SendMessageA(hwndLogBox, EM_SETSEL, length, length);
      SendMessageA(hwndLogBox, EM_REPLACESEL, 0, (LPARAM)(trimmedMsg + "\r\n").c_str());
    }

    if (!hwndLogBox || this->devMode)
    {
      printf("%s\n", trimmedMsg.c_str());
    }
  }

  // Append message to log box (overloaded for formatted strings)
  void AppendLog(const char *format, ...)
  {
    char buffer[255]; // Adjust size as needed
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);

    AppendLog(std::string(buffer));
  }

  void setKazuyaPermaDevil(uintptr_t playerAddr, int value)
  {
    if (!playerAddr || !permaDevilOffset)
      return;
    game.write<int>(playerAddr + permaDevilOffset, value);
  }

  void setHeihachiPermaWI(uintptr_t playerAddr, int value)
  {
    if (!playerAddr || !heihachiWIOffset)
      return;
    game.write<int>(playerAddr + heihachiWIOffset, value);
  }

  // Checks if it's eligible to load the boss character
  bool isEligible(uintptr_t matchStructAddr)
  {
    int value = game.readInt32(matchStructAddr);
    return value == 1 || value == 2 || value == 4 || value == 5 || value == 6 || value == 12 || value == 18;
  }

  // Checks if it's eligible to load the boss character
  // This one will be used by the hook. Story mode already loads all the extra cameras, so no need to load them again.
  bool isEligible__ExcludeStory(uintptr_t matchStructAddr)
  {
    int value = game.readInt32(matchStructAddr);
    return value == 1 || value == 2 || value == 4 || value == 6 || value == 12;
  }

  // Side: 0 = P1, 1 = P2
  uintptr_t getPlayerAddress(int side)
  {
    return game.getAddress({(DWORD)playerStructOffset, (DWORD)(0x30 + side * 8)});
  }

  uintptr_t getMovesetAddress(uintptr_t playerAddr)
  {
    return game.ReadUnsignedLong(playerAddr + movesetOffset);
  }

  int getCharId(uintptr_t playerAddr)
  {
    return game.readInt32(playerAddr + 0x168);
  }

  // Side: 0 = P1, 1 = P2
  int getCharId(uintptr_t matchStructAddr, int side)
  {
    return game.readInt32(matchStructAddr + 0x10 + side * 0x84);
  }

  // Side: 0 = P1, 1 = P2
  void setCharId(uintptr_t matchStructAddr, int side, int charId)
  {
    game.write(matchStructAddr + 0x10 + side * 0x84, charId);
  }

  int getCode(int selectedSide)
  {
    return selectedSide ? this->bossCode_R : this->bossCode_L;
  }

  bool movesetExists(uintptr_t moveset)
  {
    std::string str = game.ReadString(moveset + 8, 3);
    return str.compare("ALI") == 0 || str.compare("TEK") == 0;
  }

  bool isMovesetEdited(uintptr_t moveset)
  {
    std::string str = game.ReadString(moveset + 8, 3);
    return str.compare("ALI") == 0;
  }

  bool markMovesetEdited(uintptr_t moveset)
  {
    try
    {
      game.writeString(moveset + 8, "ALI");
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  void scanAddresses()
  {
    uintptr_t addr = 0;
    uintptr_t base = game.getBaseAddress();
    uintptr_t start = base;

    addr = game.FastAoBScan(Tekken::PLAYER_STRUCT_SIG_BYTES, start + 0x5A00000);
    if (addr != 0)
    {
      start = addr; // To use as starting point for other scans

      // $1 + $2 + $3 - $4
      // $1 = Address at which the signature bytes were found
      // $2 = Length of the instruction where signature bytes were found
      // $3 = Relative offset to Player base address within the signature instruction
      // $4 = game's base address
      playerStructOffset = addr + 7 + game.readUInt32(addr + 3) - base;
    }
    else
    {
      throw std::runtime_error("Player Struct Base Address not found!");
    }

    addr = game.FastAoBScan(Tekken::MATCH_STRUCT_SIG_BYTES, start);
    if (addr != 0)
    {
      // $1 + $2 + $3 - $4
      // $1 = Address at which the signature bytes were found
      // $2 = Length of the instruction where signature bytes were found
      // $3 = Relative offset to Player base address within the signature instruction
      // $4 = game's base address
      matchStructOffset = addr + 7 + game.readUInt32(addr + 3) - base;
    }
    else
    {
      throw std::runtime_error("Match Struct Base Address not found!");
    }

    // We no longer use it since I've recreated the decryption method
    // addr = game.FastAoBScan(Tekken::ENC_SIG_BYTES, base + 0x1700000);
    // if (addr != 0)
    // {
    //   decryptFuncAddr = addr;
    // }
    // else
    // {
    //   throw std::runtime_error("Decryption Function Address not found!");
    // }

    addr = game.FastAoBScan(Tekken::HUD_ICON_SIG_BYTES, start);
    if (addr != 0)
    {
      hudIconAddr = addr + 13;
      const uint16_t iconBytes = game.readUInt16(hudIconAddr);
      if (iconBytes != HUD_ICON_ORIG && iconBytes != HUD_PATCH_NOP)
        hudIconAddr = 0;
    }
    else
    {
      hudIconAddr = 0;
    }

    if (hudIconAddr)
    {
      addr = game.FastAoBScan(Tekken::HUD_NAME_SIG_BYTES, addr + 0x10, addr + 0x1000);
      if (addr != 0)
      {
        hudNameAddr = addr + 13;
        const uint16_t nameBytes = game.readUInt16(hudNameAddr);
        if (nameBytes != HUD_NAME_ORIG && nameBytes != HUD_PATCH_NOP)
          hudNameAddr = 0;
      }
      else
      {
        hudNameAddr = 0;
      }
    }
    else
    {
      hudNameAddr = 0;
    }

    handleIcons = hudIconAddr && hudNameAddr;
    if (!handleIcons)
    {
      AppendLog("HUD Icon/Name addresses not found (custom HUD icons disabled)");
    }
    // handleIcons = false; // For Debugging

    addr = game.FastAoBScan(Tekken::MOVSET_OFFSET_SIG_BYTES, base + 0x1700000);
    if (addr != 0)
    {
      movesetOffset = game.readUInt32(addr + 3);
    }
    else
    {
      throw std::runtime_error("\"Moveset\" Offset not found!");
    }

    addr = game.FastAoBScan(Tekken::DEVIL_FLAG_SIG_BYTES, base + 0x1900000);
    if (addr != 0)
    {
      permaDevilOffset = game.readUInt32(addr + 2);
    }
    else
    {
      throw std::runtime_error("\"Permanent Devil Mode\" offset not found!");
    }

    addr = game.FastAoBScan(Tekken::HEI_WI_SIG_BYTES, base + 0x5C00000);
    if (addr != 0)
    {
      heihachiWIOffset = game.readUInt32(addr + 3);
    }
    else
    {
      throw std::runtime_error("\"Heihachi Warrior Instinct\" offset not found!");
    }

    // AoB starts at function prologue — Story RA camera hook injection point
    addr = game.FastAoBScan(Tekken::STORY_CAMERA_HOOK_SIG_BYTES, base + 0x5C00000);
    if (addr != 0)
    {
      cameraHookAddr = addr;
    }
    else
    {
      cameraHookAddr = 0;
      AppendLog("Story Camera Hook Address not found (camera remap disabled)");
    }

    // AoB starts at function prologue — Drama camera hook (Devil Jin intro/winpose)
    addr = game.FastAoBScan(Tekken::DRAMA_CAMERA_HOOK_SIG_BYTES, base + 0x5C00000);
    if (addr != 0)
    {
      dramaCameraHookAddr = addr;
    }
    else
    {
      dramaCameraHookAddr = 0;
      AppendLog("Drama Camera Hook Address not found (intro/winpose remap disabled)");
    }

    // Picks up fighters released after this build; falls back to the built-in
    // codes if the scan cannot be validated.
    std::string charCodeStatus;
    int newFighters = scanFighterCodes(game, charCodeStatus);
    if (newFighters != 0 || devMode)
    {
      AppendLog(charCodeStatus);
    }

    if (devMode)
    {
      printf("playerStructOffset: 0x%llX\n", playerStructOffset);
      printf("matchStructOffset: 0x%llX\n", matchStructOffset);
      // printf("decryptFuncAddr: 0x%llX\n", decryptFuncAddr);
      printf("hudIconAddr: 0x%llX\n", hudIconAddr);
      printf("hudNameAddr: 0x%llX\n", hudNameAddr);
      printf("movesetOffset: 0x%llX\n", movesetOffset);
      printf("permaDevilOffset: 0x%llX\n", permaDevilOffset);
      printf("heihachiWIOffset: 0x%llX\n", heihachiWIOffset);
      printf("cameraHookAddr: 0x%llX\n", cameraHookAddr);
      printf("dramaCameraHookAddr: 0x%llX\n", dramaCameraHookAddr);
    }
    this->ready = true; // Ready to load bosses
  }

  // Modifies the instructions that allows for custom HUD icon loading.
  // Sites may already be NOP'd (sig masks match either original jz or 90 90).
  void modifyHudAddr(uintptr_t matchStructAddr)
  {
    int mode = game.readInt32(matchStructAddr);
    if (mode != 1 && mode != 6)
      return;
    if (!hudIconAddr || !hudNameAddr)
      return;

    const uint16_t icon = game.readUInt16(hudIconAddr);
    if (icon == HUD_ICON_ORIG)
      game.write<uint16_t>(hudIconAddr, HUD_PATCH_NOP);

    const uint16_t name = game.readUInt16(hudNameAddr);
    if (name == HUD_NAME_ORIG)
      game.write<uint16_t>(hudNameAddr, HUD_PATCH_NOP);
  }

  void restoreHudAddr(uintptr_t matchStructAddr)
  {
    (void)matchStructAddr;
    if (!hudIconAddr || !hudNameAddr)
      return;

    if (game.readUInt16(hudIconAddr) == HUD_PATCH_NOP)
      game.write<uint16_t>(hudIconAddr, HUD_ICON_ORIG);

    if (game.readUInt16(hudNameAddr) == HUD_PATCH_NOP)
      game.write<uint16_t>(hudNameAddr, HUD_NAME_ORIG);
  }

  void loadBossHud(uintptr_t matchStruct, int side, int charId, int bossCode)
  {
    if (bossCode == BossCodes::None)
      return;
    char icon[256]{};
    char name[256]{};
    const char c = side == 0 ? 'L' : 'R';
    bool isStoryDvj = isValidDevilJinBoss(bossCode) && charId == FighterId::DevilJin2;
    if (isStoryDvj)
    {
      if (bossCode == BossCodes::DevilJin || bossCode == BossCodes::DevilJin_1) {
        buildIconPath(icon, sizeof(icon), c, getCharCode(FighterId::Jin));
        buildNamePath(name, sizeof(name), FighterId::Jin);
      }
      else if (bossCode == BossCodes::DevilJin_2) {
        buildIconPath(icon, sizeof(icon), c, HudIcon::DvjCh12);
        buildNamePath(name, sizeof(name), FighterId::DevilJin);
      }
      else if (bossCode == BossCodes::DevilJin_3) {
        buildIconPath(icon, sizeof(icon), c, HudIcon::DvjCh13);
        buildNamePath(name, sizeof(name), FighterId::DevilJin);
      }
    } 
    else if ((bossCode == BossCodes::FinalJin || bossCode == BossCodes::MishimaJin || bossCode == BossCodes::KazamaJin) && charId == FighterId::Jin)
    {
      buildIconPath(icon, sizeof(icon), c, HudIcon::JinFinal);
      buildNamePath(name, sizeof(name), FighterId::Jin);
    }
    else if (bossCode == BossCodes::FinalKazuya && charId == FighterId::Kazuya)
    {
      buildIconPath(icon, sizeof(icon), c, HudIcon::KazFinal);
      buildNamePath(name, sizeof(name), FighterId::Kazuya);
    }
    else if (bossCode == BossCodes::DevilKazuya && charId == FighterId::Kazuya)
    {
      buildIconPath(icon, sizeof(icon), c, HudIcon::KazDevil);
      buildNamePath(name, sizeof(name), HudName::KazDevil);
    }
    else if (bossCode == BossCodes::AmnesiaHeihachi && charId == FighterId::Heihachi)
    {
      buildIconPath(icon, sizeof(icon), c, HudIcon::HeiMonk);
      buildNamePath(name, sizeof(name), FighterId::Heihachi);
    }
    else if (bossCode == BossCodes::ShadowHeihachi && charId == FighterId::Heihachi)
    {
      buildIconPath(icon, sizeof(icon), c, HudIcon::HeiShadow);
      buildNamePath(name, sizeof(name), HudName::HeiShadow);
    }
    if (icon[0] && (shouldHandleHudAndCostumes() || isStoryDvj))
      game.writeString(matchStruct + 0x2C0 + side * 0x100, icon, HUD_PATH_MAX);
    if (name[0] && (shouldHandleHudAndCostumes() || isStoryDvj))
      game.writeString(matchStruct + 0x4C0 + side * 0x100, name, HUD_PATH_MAX);
  }

  void hudHandler(uintptr_t matchStruct)
  {
    int char1 = game.readInt32(matchStruct + 0x10);
    int char2 = game.readInt32(matchStruct + 0x94);
    char icon1[256]{};
    char icon2[256]{};
    char name1[256]{};
    char name2[256]{};
    getIconPath(icon1, sizeof(icon1), 0, char1);
    getIconPath(icon2, sizeof(icon2), 1, char2);
    buildNamePath(name1, sizeof(name1), char1);
    buildNamePath(name2, sizeof(name2), char2);
    game.writeString(matchStruct + 0x2C0, icon1, HUD_PATH_MAX);
    game.writeString(matchStruct + 0x3C0, icon2, HUD_PATH_MAX);
    game.writeString(matchStruct + 0x4C0, name1, HUD_PATH_MAX);
    game.writeString(matchStruct + 0x5C0, name2, HUD_PATH_MAX);

    loadBossHud(matchStruct, 0, char1, this->bossCode_L);
    loadBossHud(matchStruct, 1, char2, this->bossCode_R);
  }

  void loadCharacter(uintptr_t matchStructAddr, int side, int bossCode)
  {
    int charId = -1;
    int currCharId = getCharId(matchStructAddr, side);
    switch (bossCode)
    {
    // In these cases, the bossCode is the chara ID itself
    case BossCodes::Azazel:
    case BossCodes::AngelJin:
    case BossCodes::TrueDevilKazuya:
    case BossCodes::DevilJin:
      charId = bossCode;
      break;
    case BossCodes::DevilJin_1:
    case BossCodes::DevilJin_2:
    case BossCodes::DevilJin_3:
      charId = FighterId::DevilJin2;
      break;
    default:
      return;
    }
    if (charId != -1 && isEligible(matchStructAddr))
    {
      setCharId(matchStructAddr, side, charId);
      // if (charId == FighterId::DevilJin2)
      // {
      //   loadCostume(matchStructAddr, side, 51, DEVIL_JIN_COSTUME_PATH); // Just a safety precaution
      // }
    }
  }

  void loadCostume(uintptr_t matchStructAddr, int side, int costumeId, const char *costumePath)
  {
    game.write<int>(matchStructAddr + 0x6F0 + side * 0x6760, costumeId);
    game.writeString(matchStructAddr + 0x13D78 + side * 0x100, costumePath, 255);
  }

  void costumeHandler(uintptr_t matchStructAddr, int side, int bossCode)
  {
    if (!matchStructAddr)
      return;
    if (!isCorrectCharacter(bossCode, getCharId(matchStructAddr, side)))
      return;
    const char *costumePath = nullptr;
    switch (bossCode)
    {
    // case BossCodes::RegularJin:
    // case BossCodes::NerfedJin:
    // case BossCodes::DevilKazuya:
    // case BossCodes::FinalHeihachi:
    case BossCodes::Azazel:
    case BossCodes::AngelJin:
    case BossCodes::TrueDevilKazuya:
      return loadCostume(matchStructAddr, side, 0, "");
    case BossCodes::ChainedJin:
      costumePath = CHAINED_JIN_COSTUME_PATH;
      break;
    case BossCodes::MishimaJin:
    case BossCodes::KazamaJin:
    case BossCodes::FinalJin:
      costumePath = FINAL_JIN_COSTUME_PATH;
      break;
    case BossCodes::FinalKazuya:
      costumePath = FINAL_KAZ_COSTUME_PATH;
      break;
    case BossCodes::DevilJin:
    case BossCodes::DevilJin_1:
      costumePath = DEVIL_JIN_COSTUME_PATH;
      break;
    case BossCodes::DevilJin_2:
      costumePath = DEVIL_JIN_COSTUME_PATH_2;
      break;
    case BossCodes::DevilJin_3:
      costumePath = DEVIL_JIN_COSTUME_PATH_3;
      break;
    case BossCodes::AmnesiaHeihachi:
      costumePath = HEIHACHI_MONK_COSTUME_PATH;
      break;
    case BossCodes::ShadowHeihachi:
      costumePath = HEIHACHI_SHADOW_COSTUME_PATH;
      break;
    default:
      return;
    }

    if (!shouldHandleHudAndCostumes() && !isValidDevilJinBoss(bossCode))
      return;

    loadCostume(matchStructAddr, side, 51, costumePath);
  }

  void adjustIntroOutroReq(TkMoveset &moveset, int bossCode, int start = 0)
  {
    uintptr_t reqHeader = moveset.getMovesetHeader("requirements");
    uintptr_t reqCount = moveset.getMovesetCount("requirements");
    uintptr_t requirement = 0;
    int req = -1;
    for (int i = start; i < reqCount; i++)
    {
      requirement = reqHeader + i * sizeof(TK_Requirement);
      req = game.readInt32(requirement);
      if (req == Requirements::FATE_INTRO_RELATED)
      {
        game.write(requirement + 4, bossCode);
      }
    }
  }

  void handleHeihachiMoveProp(uintptr_t movesetAddr, int moveIdx)
  {
    TkMoveset moveset(this->game, movesetAddr, this->decryptFuncAddr);
    uintptr_t addr = moveset.getMoveAddrByIdx(moveIdx);
    addr = moveset.getMoveExtrapropAddr(addr);
    while (true)
    {
      int frame = moveset.getExtrapropValue(addr, "frame");
      int prop = moveset.getExtrapropValue(addr, "prop");
      if (!prop && !frame)
        break;
      if (prop == ExtraMoveProperties::SPEND_RAGE)
      {
        moveset.editExtrapropValue(addr, "value", 0); // don't spend rage
      }
      if (prop == ExtraMoveProperties::HEAT_RELATED)
      {
        moveset.editExtrapropValue(addr, "value", 300);
      }
      if (prop == ExtraMoveProperties::SHORT_FLAG)
      {
        moveset.editExtrapropValue(addr, "value", 0x220001);
      }
      // Cancels & Props both have requirements at offset 0x8
      if (moveset.cancelHasCondition(addr, Requirements::DLC_STORY1_BATTLE_NUM))
      {
        moveset.disableStoryRelatedReqs(moveset.getExtrapropValue(addr, "requirements"));
      }
      addr += sizeof(TK_ExtraProp);
    }
  }

  // --- Jin boss helpers (used by loadJin) ---

  // ChainedJin (11) uses story-flag param 1 — same unlock pool as NerfedJin.
  void applyJinStoryRequirements(TkMoveset &moveset, int bossCode)
  {
    int storyFlagParam = bossCode == BossCodes::ChainedJin ? 1 : bossCode;
    uintptr_t start = moveset.getMovesetHeader("requirements");
    uintptr_t count = moveset.getMovesetCount("requirements");
    for (size_t i = 0; i < count; i++)
    {
      uintptr_t addr = start + i * sizeof(TK_Requirement);
      TK_Requirement requirement = moveset.getRequirement(addr);
      if (requirement.req == Requirements::STORY_FLAGS && requirement.param[0] == storyFlagParam)
      {
        game.write<int>(addr, 0);
      }
      else if (requirement.req == Requirements::NOT_STORY_MODE)
      {
        game.write<int>(addr, Requirements::STORY_FLAGS);
      }
    }
  }

  // Rage Art Camera (requires Assembly Injection)
  void applyJinRageArtCameras(TkMoveset &moveset)
  {
    if (!INSTALL_CAMERA_HOOKS || !cameraHookInstalled)
      return;

    auto setRageArtCamera = [&](uint32_t nameKey, int value)
    {
      uintptr_t addr = moveset.getMoveAddress(nameKey, moveset.getAliasMoveId(0x8000) - 20);
      addr = moveset.getMoveExtrapropAddr(addr);
      addr = moveset.findExtraProp(addr, ExtraMoveProperties::RAGE_ART_CAMERA);
      if (addr)
      {
        moveset.editExtrapropValue(addr, "value", value);
      }
    };

    setRageArtCamera(0x9bae061e, 5); // Jz_Story_RageArts00
    setRageArtCamera(0x22e4beeb, 5); // Jz_RageArts01_St
    setRageArtCamera(0x5898e42a, 6); // Jz_RageArts_n_St
  }

  // Season 2: replace f,f+1+2 with f,f+2; adjust FC df4 ~ ZEN cancels
  void applyJinSeason2Fixes(TkMoveset &moveset)
  {
    {
      int ff2 = moveset.getMoveId(0xE383D012, 2200);  // f,f+2
      int ff12 = moveset.getMoveId(0xEB242623, 1750); // f,f+1+2
      std::vector<std::pair<int, int>> moves = {{ff12, ff2}};
      moveset.replaceCancelMoveIndexes(moves);
    }

    uintptr_t addr = moveset.getMoveAddress(0x8c0f6a17, 1600); // Jz_zan_srk00EX_zan
    if (!addr)
      return;

    uintptr_t firstCancel = moveset.getMoveNthCancel(addr, 0);
    uintptr_t cancel = 0;
    int moveId = -1;
    short tMoveId = -1;

    // Replacing df from that ZEN with the story version
    moveId = moveset.getMoveId(0xda8608b7, 1790); // Jz_shoryu_P
    cancel = moveset.findCancel(firstCancel, "move", moveId);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0x39b5f537, 2200);
      moveset.editCancelValue(cancel, "move", tMoveId);
    }

    // ZEN 1+2 becomes ZEN u+1+2 because of command priority
    cancel = moveset.findCancel(firstCancel, "command", 0x4000000300000000);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0x91130746, 2300); // ZEN u+1+2
      moveset.editCancelValue(cancel, "command", 0x4000000300000300);
      moveset.editCancelValue(cancel, "requirement_idx", 0);
      moveset.editCancelValue(cancel, "move", tMoveId); // ZEN u+1+2
    }

    // Adjusting ZEN 3+4
    moveId = moveset.getMoveId(0x362078c4, 1820); // ZEN 3+4
    cancel = moveset.findCancel(firstCancel, "move", moveId);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0x1a53432b, 2300); // ZEN u+1+2
      moveset.editCancelValue(cancel, "move", tMoveId);
    }

    // Disabling ZEN u+1
    moveId = moveset.getMoveId(0xc69959b0, 1580); // ZEN u+1
    cancel = moveset.findCancel(firstCancel, "move", moveId);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0xb235481b, 1600); // ZEN 1+2
      moveset.editCancelValue(cancel, "command", 0x4000000300000000);
      moveset.editCancelValue(cancel, "move", tMoveId);
    }

    // Adjusting ZEN 1
    moveId = moveset.getMoveId(0xea6240d3, 1580); // ZEN 1
    cancel = moveset.findCancel(firstCancel, "move", moveId);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0x69655f3c, 2300);
      moveset.editCancelValue(cancel, "move", tMoveId);
    }

    // Adjusting ZEN 2
    moveId = moveset.getMoveId(0xc48dd080, 1580); // ZEN 2
    cancel = moveset.findCancel(firstCancel, "move", moveId);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0xa34e66df, 2300);
      moveset.editCancelValue(cancel, "extradata", moveset.findCancelExtradata(389));
      moveset.editCancelValue(cancel, "move", tMoveId);
    }

    // Adjusting ZEN 4
    moveId = moveset.getMoveId(0xfd3fe1a6, 1800); // ZEN 2
    cancel = moveset.findCancel(firstCancel, "move", moveId);
    if (cancel)
    {
      tMoveId = moveset.getMoveId(0xc2c9eadc, 2300);
      moveset.editCancelValue(cancel, "move", tMoveId);
    }
  }

  void applyNonMishimaJinFixes(TkMoveset &moveset)
  {
    // Solving the "ws+1, [3,3] ~ df" bug (skipped for MishimaJin)
    uintptr_t addr = moveset.getMoveAddress(0x530890fb, 0x8000); // Jz_Mishima_Std_3LKsLK
    addr = moveset.getMoveNthCancel(addr, 2);
    int Jz_shoryu_shift = moveset.getMoveId(0x459c84c1, 1800);
    moveset.editCancelValue(addr, "move", Jz_shoryu_shift);

    // `f, n, df1` CD1 fix, apply story variant
    std::vector<std::pair<int, int>> moves = {
      {
        moveset.getMoveId(0x27a2625e, Jz_shoryu_shift), // Jz_dslpS
        moveset.getMoveId(0x38391750, 0x8000), // Jz_Story_623_LP_fast
      },
    };
    moveset.replaceCancelMoveIndexes(moves);
  }

  // EWGF > OTGF bug fix. Only Final Jin should keep OTGF; other variants redirect to CD+1
  void fixJinEwgfOtgfCancel(TkMoveset &moveset)
  {
    uintptr_t addr = moveset.getMoveAddress(0x39b5f537, 0x8000);
    addr = moveset.getMoveNthCancel(addr, 0);
    addr = moveset.findCancel(addr, "command", 0x4000000300000000);
    int moveId = moveset.getCancelValue(moveset.iterateCancel(addr, 1), "move"); // Get CD+1 ID from next cancel
    moveset.editCancelValue(addr, "move", moveId); // Jz_Story_623_LP_fast
  }

  void installJinStanceAlias(TkMoveset &moveset, uintptr_t movesetAddr, uint32_t nameKey)
  {
    int moveId = moveset.getMoveId(nameKey, 2000);
    if (moveId != 0)
    {
      game.write<short>(movesetAddr + 0xAA, moveId);
    }
  }

  void applyRegularJinExclusive(TkMoveset &moveset)
  {
    uintptr_t addr = moveset.getMoveAddress(0x9b789d36, 1865); // d/b+1+2
    addr = moveset.getMoveNthCancel(addr, 0);
    moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"), 0);
  }

  void applyFinalJinExclusive(TkMoveset &moveset)
  {
    if (shouldDisableAutoParries())
    {
      uintptr_t addr = moveset.getMoveAddrByIdx(0x8001);
      int targetMoveId = moveset.getMoveId(0xc2da6f70, 2500);
      uintptr_t cancel = moveset.findCancel(moveset.getMoveNthCancel(addr), "move", targetMoveId);
      if (cancel)
      {
        for (int i = 0; i < 4; i++)
        {
          uintptr_t reqAddr = moveset.getCancelValue(cancel, "requirements");
          moveset.editRequirement(reqAddr, Requirements::STORY_BATTLE);
          cancel = moveset.iterateCancel(cancel, 1);
        }
      }
    }

    int Jz_karate01 = moveset.getMoveId(0xf60501d3, 1800);
    int Jz_FinalKarate_Std_46RP = moveset.getMoveId(0x60335bf7, 2400);
    std::vector<std::pair<int, int>> moves = {{Jz_karate01, Jz_FinalKarate_Std_46RP}};
    moveset.replaceCancelMoveIndexes(moves);
  }

  void applyMishimaJinExclusive(TkMoveset &moveset, uintptr_t movesetAddr)
  {
    installJinStanceAlias(moveset, movesetAddr, 0xA33CD19D);

    int moveId1 = moveset.getMoveId(0xf60501d3, 1800); // Jz_karate01
    int moveId2 = moveset.getMoveId(0x075ae247, 2350); // Jz_Mishima_Std_46RP

    // Left: Target. Right: Replacement
    std::vector<std::pair<int, int>> moves = {
        {
            moveId1,
            moveId2,
        },
        {
            moveset.getMoveId(0x27a2625e, moveId1), // Jz_dslpS
            moveset.getMoveId(0xa668cc41, moveId2), // Jz_Mishima_623_LP
        },
        {
            moveset.getMoveId(0x38391750, 0x8000),  // Jz_Story_623_LP_fast
            moveset.getMoveId(0xa668cc41, moveId2), // Jz_Mishima_623_LP
        },
        {
            moveset.getMoveId(0xc5f63209, 2300),    // Jz_Karate_623_RK
            moveset.getMoveId(0xc52dcc87, moveId2), // Jz_Mishima_623_RK
        },
        {
            moveset.getMoveId(0x0d4044cc, moveId1 + 50), // Jz_shoryu24
            moveset.getMoveId(0x88805a0e, moveId2),      // Jz_Mishima_623_RP_fast
        },
    };

    moveset.replaceCancelMoveIndexes(moves);
  }

  void applyKazamaJinExclusive(TkMoveset &moveset, uintptr_t movesetAddr)
  {
    installJinStanceAlias(moveset, movesetAddr, 0x7614EF15);

    // Disabling 1+4 > 1+2 cancel, this shouldn't exist for Kazama-Jin
    {
      // Take the Idx and apply it to appropriate places
      int moveId = moveset.getMoveId(0xfe41a93c, 2400); // Jz_Kazama_Std_LPRK2
      uintptr_t addr = moveset.getMoveAddrByIdx(moveId);
      addr = moveset.getMoveNthCancel(addr, 0);
      int reqIdx = moveset.getCancelValue(addr, "requirement_idx");

      addr = moveset.getMoveAddress(0xd4095da0, moveId - 10); // Jz_Kazama_Std_LPRK
      addr = moveset.getMoveNthCancel(addr, 0);
      while (true)
      {
        uintptr_t command = moveset.getCancelValue(addr, "command");
        if (command == 0x8000)
          break;
        // For 1+2 command, place the above acquired req_idx to disable these cancels
        if (command == 0x4000000300000000)
          moveset.editCancelValue(addr, "requirement_idx", reqIdx);
        addr = moveset.iterateCancel(addr, 1);
      }
    }

    if (!shouldDisableAutoParries())
      return;

    // 4 moves' cancel list should be updated to remove any cancels to parries
    // Jz_sWALKB (1489)
    // Jz_sKAM00_Ac15B3 (2454)
    // Jz_Kazama_sWALKB (2456)
    // Jz_Kazama_sWALKF (2457)

    int Jz_sKAM00_Ac15B3 = moveset.getMoveId(0x7614ef15, 2400);
    int Jz_Kazama_Atemi_throw_LP_nage = moveset.getMoveId(0xe94d6d9a, Jz_sKAM00_Ac15B3); // Jz_Kazama_Atemi_throw_LP_nage
    const std::array<int, 4> moves = {
        Jz_Kazama_Atemi_throw_LP_nage,
        moveset.getMoveId(0x826a36b7, Jz_Kazama_Atemi_throw_LP_nage), // Jz_Kazama_Atemi_throw_RP_nage
        moveset.getMoveId(0xc3750ffb, Jz_Kazama_Atemi_throw_LP_nage), // Jz_Kazama_Atemi_throw_LK_nage
        moveset.getMoveId(0x67008eea, Jz_Kazama_Atemi_throw_LP_nage), // Jz_Kazama_Atemi_throw_RK_nage
    };

    auto disableParryCancels = [&](uint32_t nameKey, int startParam)
    {
      uintptr_t addr = moveset.getMoveAddress(nameKey, startParam);
      addr = moveset.getMoveNthCancel(addr, 0);
      if (!addr)
        return;
      while (moveset.getCancelValue(addr, "command") != 0x8000)
      {
        if (!addr)
          return;
        const int cMoveId = moveset.getCancelValue(addr, "move");
        if (std::find(moves.begin(), moves.end(), cMoveId) != moves.end())
        {
          moveset.editCancelValue(addr, "start", 0x7FFF);
        }
        addr = moveset.iterateCancel(addr, 1);
      }
    };

    disableParryCancels(0x6e61c919, 0x8001); // Jz_sWALKB
    disableParryCancels(0x7614ef15, Jz_sKAM00_Ac15B3);
    disableParryCancels(0x15088ae1, Jz_sKAM00_Ac15B3); // Jz_Kazama_sWALKB
    disableParryCancels(0x87ca9ecf, Jz_sKAM00_Ac15B3); // Jz_Kazama_sWALKF
  }

  void applyChainedJinExclusive(TkMoveset &moveset)
  {
    std::vector<std::pair<int, int>> moves = {
        {0xCAD0CF3C, 1500}, // 1+2
        {0xE383D012, 2000}, // f,f+2
        {0xEEE71DFB, 1400}, // b+1+2
        {0x9B789D36, 1600}  // d/b+1+2
    };

    for (const auto &move : moves)
    {
      uintptr_t moveAddr = moveset.getMoveAddress(move.first, move.second);
      if (moveAddr)
      {
        uintptr_t cancel = moveset.getMoveNthCancel(moveAddr, 0);
        moveset.editCancelValue(cancel, "requirement_idx", 0);
      }
    }
  }

  bool loadJin(uintptr_t movesetAddr, int bossCode)
  {
    if (!isValidJinBoss(bossCode))
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);

    applyJinStoryRequirements(moveset, bossCode);

    if (bossCode != BossCodes::RegularJin)
    {
      applyJinRageArtCameras(moveset);
      applyJinSeason2Fixes(moveset);
    }
    if (bossCode != BossCodes::MishimaJin)
      applyNonMishimaJinFixes(moveset);
    // Only Final Jin should keep EWGF > OTGF
    if (bossCode != BossCodes::FinalJin)
      fixJinEwgfOtgfCancel(moveset);

    switch (bossCode)
    {
    case BossCodes::RegularJin:
      applyRegularJinExclusive(moveset);
      break;
    case BossCodes::NerfedJin:
      // Story flag param 1 already applied in applyJinStoryRequirements; no exclusive edits
      break;
    case BossCodes::FinalJin:
      applyFinalJinExclusive(moveset);
      break;
    case BossCodes::MishimaJin:
      applyMishimaJinExclusive(moveset, movesetAddr);
      break;
    case BossCodes::KazamaJin:
      applyKazamaJinExclusive(moveset, movesetAddr);
      break;
    case BossCodes::ChainedJin:
      applyChainedJinExclusive(moveset);
      break;
    default:
      return false;
    }
    return markMovesetEdited(movesetAddr);
  }

  bool loadKazuya(uintptr_t movesetAddr, int bossCode)
  {
    if (!isValidKazuyaBoss(bossCode))
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    int idleStanceIdx = moveset.getAliasMoveId(0x8001);
    if (bossCode == BossCodes::DevilKazuya)
    {
      // Enabling permanent Devil form
      uintptr_t addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.editExtraprop(addr, ExtraMoveProperties::PERMA_DEVIL, 1);

      addr = moveset.getMoveAddress(0x42CCE45A, idleStanceIdx); // CD+4, 1 last hit key
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE, -1);
      if (addr != 0)
      {
        moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
        addr = moveset.iterateCancel(addr, 2); // Move 2 cancels forward
        moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      }

      // 1,1,2
      addr = moveset.getMoveAddress(0x2226A9EE, idleStanceIdx);
      // 3rd cancel
      addr = moveset.getMoveNthCancel(addr, 2);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));

      // Juggle Escape
      addr = moveset.getMoveAddress(0xDEBED999, 5);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      // 6th cancel
      moveset.disableStoryRelatedReqs(addr);
      // 7th cancel
      moveset.disableStoryRelatedReqs(moveset.iterateCancel(addr, 1));

      // f,f+2 (story version)
      addr = moveset.getMoveAddress(0x1A571FA1, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      addr += Sizes::Moveset::Cancel; // Move 1 cancel forward
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));

      // d/b+1+2
      addr = moveset.getMoveAddress(0x73EBDBA2, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      moveset.editCancelValue(addr, "requirement_idx", 0);

      // d/b+4
      addr = moveset.getMoveAddress(0x9364E2F5, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      addr = moveset.getCancelValue(addr, "requirements");
      moveset.disableStoryRelatedReqs(addr);
      // Disabling standing req
      game.write<int>(addr + sizeof(TK_Requirement), 0);

      return markMovesetEdited(movesetAddr);
    }
    else if (bossCode == BossCodes::FinalKazuya)
    {
      // Go through reqs and props to disable his devil form
      // requirements
      uintptr_t start = moveset.getMovesetHeader("requirements");
      uintptr_t count = moveset.getMovesetCount("requirements");
      for (uintptr_t i = 4530; i < count - 2000; i++)
      {
        uintptr_t addr = start + (i * sizeof(TK_Requirement));
        TK_Requirement requirement = moveset.getRequirement(addr);
        if ((requirement.req == ExtraMoveProperties::DEVIL_STATE && requirement.param[0] >= 1) ||
            (requirement.req == ExtraMoveProperties::WING_ANIM))
        {
          moveset.editRequirement(addr, 0, 0);
        }
      }

      // extraprops
      start = moveset.getMovesetHeader("extra_move_properties");
      count = moveset.getMovesetCount("extra_move_properties");

      for (uintptr_t i = 2200; i < count; i++)
      {
        uintptr_t addr = start + (i * sizeof(TK_ExtraProp));
        int prop = moveset.getExtrapropValue(addr, "prop");
        int param = moveset.getExtrapropValue(addr, "value");
        if (prop == ExtraMoveProperties::DEVIL_STATE || prop == ExtraMoveProperties::WING_ANIM || (prop == ExtraMoveProperties::CHARA_TRAIL_VFX && (param == 0xC || param == 0xD)))
        {
          moveset.editExtraprop(addr, 0, 0);
        }
      }

      // f, f+2
      {
        uintptr_t addr = moveset.getMoveAddress(0xC00BB85A, idleStanceIdx);
        int idx = moveset.getMoveIdxByAddress(addr);
        int moveId = moveset.getMoveId(0xbe4863c0, idx + 1); // Kz_66rp_DVL
        addr = moveset.getMoveNthCancel(addr, 0);
        uintptr_t reqHeader = moveset.getMovesetHeader("requirements");
        moveset.editCancelValue(addr, "requirement_idx", 0);
        moveset.editCancelValue(addr, "extradata", moveset.findCancelExtradata(1025));
        moveset.editCancelValue(addr, "start", 1);
        moveset.editCancelValue(addr, "end", 1);
        moveset.editCancelValue(addr, "transition", 1);
        moveset.editCancelValue(addr, "move", moveId);
        moveset.editCancelValue(addr, "option", 65);
      }

      // Replacing Rage Art with Tekken-Ball counterpart
      if (config->finalKazuyaRageBlast) {
        // Kz_RageArts00
        uintptr_t addr = moveset.getMoveAddress(0xfaf65ab0, defaultAliasIdx - 100);
        int moveId = moveset.getMoveId(0xffd3c168, idleStanceIdx - 100); // Tekken Ball blast
        addr = moveset.getMoveNthCancel(addr, 0);
        moveset.editCancelValue(addr, "requirement_idx", 0);
        moveset.editCancelValue(addr, "extradata", moveset.findCancelExtradata(1025));
        moveset.editCancelValue(addr, "start", 1);
        moveset.editCancelValue(addr, "end", 1);
        moveset.editCancelValue(addr, "transition", 1);
        moveset.editCancelValue(addr, "move", moveId);
        moveset.editCancelValue(addr, "option", 65);
      }

      // Single-spin uppercut
      uintptr_t addr = moveset.getMoveAddress(0xD172C176, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelValue(addr, "command", 0x10);
      moveset.editCancelValue(addr, "option", 0x50);

      uintptr_t reqHeader = moveset.getMovesetHeader("requirements");

      // Ultra-wavedash
      addr = moveset.getMoveAddress(0x77314B09, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelValue(addr, "requirement_idx", 0);

      // CD+1+2
      addr = moveset.getMoveAddress(0x0C9CE140, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editCancelValue(addr, "requirement_idx", 0);

      // b+2,2
      addr = moveset.getMoveAddress(0x8B5BFA6C, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editCancelValue(addr, "requirement_idx", 0);

      // NEW b+2,2. Disabling laser cancel
      addr = moveset.getMoveAddress(0x8FE28C6A, defaultAliasIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelValue(addr, "extradata", moveset.findCancelExtradata(16383));

      // Disabling u/b+1+2 laser
      addr = moveset.getMoveAddress(0x07F32E0C, 2000);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editCancelValue(addr, "requirement_idx", 0);
      moveset.editCancelValue(addr, "extradata", moveset.findCancelExtradata(386));
      moveset.editCancelValue(addr, "transition", 1);
      moveset.editCancelValue(addr, "move", moveset.getMoveId(0x1376C644, idleStanceIdx));
      moveset.editCancelValue(addr, "option", 65);

      // d/f+3+4, 1
      {
        int df34_1 = moveset.getMoveId(0x6562FA84, idleStanceIdx);
        addr = moveset.getMoveAddrByIdx(df34_1);
        addr = moveset.getMoveNthCancel(addr, 0);
        int df34_1_db2 = moveset.getCancelMoveId(addr);
        int df34_1_2 = moveset.getMoveId(0xD63CD0E6, df34_1);
        addr = moveset.findCancel(addr, "move", df34_1_2);
        moveset.editCancelValue(addr, "move", df34_1_db2);
        moveset.editCancelValue(moveset.iterateCancel(addr, 1), "move", df34_1_db2);
      }

      // d/b+1, 2
      addr = moveset.getMoveAddress(0xFE501006, idleStanceIdx); // Co_t_slp00EX
      addr = moveset.getMoveNthCancel(addr, 0);
      // Grabbing move ID from 3rd cancel
      int moveId_db2 = moveset.getCancelMoveId(moveset.iterateCancel(addr, 2));
      addr = moveset.findCancel(addr, "move", moveset.getMoveId(0xbc4e3d37, 1700)); // Kz_1lprp
      moveset.editCancelValue(addr, "start", 19);
      moveset.editCancelValue(addr, "end", 19);
      moveset.editCancelValue(addr, "transition", 19);
      moveset.editCancelValue(addr, "move", moveId_db2);

      // Next cancel
      addr = moveset.iterateCancel(addr, 1);
      moveset.editCancelValue(addr, "start", 19);
      moveset.editCancelValue(addr, "end", 19);
      moveset.editCancelValue(addr, "transition", 19);
      moveset.editCancelValue(addr, "move", moveId_db2);

      // Adjusting d/b+1+2 to cancel into this on frame-1
      int moveId_db1 = moveset.getMoveId(0xFE501006, moveId_db2);
      addr = moveset.getMoveAddress(0x73EBDBA2, moveId_db1);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editCancelValue(addr, "requirement_idx", 0);
      moveset.editCancelValue(addr, "move", moveId_db1);
      moveset.editCancelValue(addr, "extradata", moveset.findCancelExtradata(1025));

      // ws+2
      addr = moveset.getMoveAddress(0xB253E5F2, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelValue(addr, "requirement_idx", 0);
      moveset.editCancelValue(addr, "extradata", moveset.findCancelExtradata(1025));
      moveset.editCancelValue(addr, "start", 5);
      moveset.editCancelValue(addr, "end", 5);
      moveset.editCancelValue(addr, "transition", 5);
      moveset.editCancelValue(addr, "move", moveset.getMoveId(0x0AB42E52, defaultAliasIdx));
      moveset.editCancelValue(addr, "option", 65);

      // Tone down ws+2 damage
      if (config->toneDownDamage) {
        addr = moveset.getMoveAddress(0xe9f45330, defaultAliasIdx);
        addr = moveset.getMoveHitCondition(addr);
        while (true) {
          moveset.editHitConditionValue(addr, "damage", 23);
          if (moveset.isLastHitCondition(addr))
            break;
          addr = moveset.iterateHitConditions(addr, 1);
        }
      }

      return markMovesetEdited(movesetAddr);
    }

    return false;
  }

  bool loadAzazel(uintptr_t movesetAddr, int bossCode)
  {
    if (bossCode != BossCodes::Azazel)
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);

    uintptr_t addr = moveset.getMoveAddrByIdx(0x8000);
    addr = moveset.getMoveNthCancel(addr, 0);
    addr = moveset.getCancelValue(addr, "requirements"); // 1st req
    moveset.editRequirement(addr, -1, 8);
    addr = moveset.iterateRequirements(addr, 1); // 2nd req
    moveset.editRequirement(addr, Requirements::OUTRO1, 0);
    addr = moveset.iterateRequirements(addr, 1); // 3rd req
    moveset.editRequirement(addr, Requirements::OUTRO2, 0);

    return markMovesetEdited(movesetAddr);
  }

  bool loadAngelJin(uintptr_t movesetAddr, int bossCode)
  {
    if (bossCode != BossCodes::AngelJin)
      return false;
    TkMoveset moveset(this->game, movesetAddr, this->decryptFuncAddr);

    // Fix Rage Art dialogue (2 cancels)
    uintptr_t addr = moveset.getMoveAddress(0x53089f24, moveset.getAliasMoveId(0x8000) - 25); // Rage Art

    uintptr_t cancelAddr = moveset.findMoveCancelByCondition(addr, Requirements::ARCADE_BATTLE);
    addr = moveset.findRequirement(moveset.getCancelValue(cancelAddr, "requirements"), Requirements::ARCADE_BATTLE);
    moveset.editRequirement(addr, 0);

    cancelAddr = moveset.iterateCancel(cancelAddr, 1);
    addr = moveset.findRequirement(moveset.getCancelValue(cancelAddr, "requirements"), Requirements::ARCADE_BATTLE);
    moveset.editRequirement(addr, 0);

    // Adjust damage for the new CD+1
    if (config->toneDownDamage) {
      addr = moveset.getMoveAddress(0x3cbbe67a, 0x8001);
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.editExtrapropValue(addr, "value", 0);
    }

    // Setting swl_s00 as the default intro
    {
      addr = moveset.getMoveAddress(0xace34ec8); // Dj_Direct
      addr = moveset.getMoveNthCancel(addr, 1);
      int moveId = moveset.getCancelValue(addr, "move");
      if (moveId == moveset.getMoveId(0x6b59f816)) // swl_s00
      {
        addr = moveset.getCancelValue(addr, "requirements");
        addr = moveset.editRequirement(addr, Requirements::INTRO_RELATED, 0);
        addr = moveset.editRequirement(addr, Requirements::EOL, 0);
      }
    }

    return markMovesetEdited(movesetAddr);
  }

  bool loadHeihachi(uintptr_t movesetAddr, int bossCode)
  {
    if (!isValidHeihachiBoss(bossCode))
      return false;
    TkMoveset moveset(this->game, movesetAddr, this->decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    int idleStanceIdx = moveset.getAliasMoveId(0x8001);
    uintptr_t addr = moveset.getMoveAddrByIdx(idleStanceIdx);

    // Idle stance, set/disable Warrior Instinct
    // addr = moveset.iterateExtraprops(moveset.getMoveExtrapropAddr(addr), 4); // 5th prop
    // moveset.editExtraprop(addr, ExtraMoveProperties::HEI_WARRIOR, (int)(bossCode == BossCodes::FinalHeihachi));

    // Rage Art Fix
    // Rage Art Camera (requires Assembly Injection)
    auto setRageArtCamera = [&](uint32_t nameKey, int value)
    {
      if (!INSTALL_CAMERA_HOOKS || !cameraHookInstalled)
        return;
      addr = moveset.getMoveAddress(nameKey, defaultAliasIdx - 20);
      addr = moveset.getMoveExtrapropAddr(addr);
      addr = moveset.findExtraProp(addr, ExtraMoveProperties::RAGE_ART_CAMERA);

      if (addr)
      {
        moveset.editExtrapropValue(addr, "value", value);
      }
    };

    if (bossCode == BossCodes::ShadowHeihachi || bossCode == BossCodes::AmnesiaHeihachi)
    {
      setRageArtCamera(0xfb78fa92, 5); // He_RageArts01_St
      setRageArtCamera(0x140be639, 5); // He_RageArts02_St
      setRageArtCamera(0xa77873d3, 6); // He_RageArts_n_St

      // Adjusting RageArt against Kazuya & Jin & "friends"
      int targetMoveId = moveset.getMoveId(0x942c4d5c);                // He_RageArts00_St
      addr = moveset.getMoveAddress(0xde97038f, defaultAliasIdx - 30); // He_RageArts00
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editCancelMoveId(addr, (short)targetMoveId);
      moveset.editCancelReqAddr(addr, moveset.getMovesetHeader("requirements"));
    }

    if (bossCode == BossCodes::ShadowHeihachi)
    {
      addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      uintptr_t cancel1 = moveset.getMoveNthCancel(addr, 0);
      uintptr_t cancel2 = moveset.getMoveNthCancel(addr, 1);
      uintptr_t reqListCancel1 = moveset.getCancelValue(cancel1, "requirements");
      uintptr_t reqListCancel2 = moveset.getCancelValue(cancel2, "requirements");
      moveset.editCancelValue(cancel1, "requirements", reqListCancel2);
      moveset.disableStoryRelatedReqs(reqListCancel1);

      // Dialogue for RA
      // Disabling regular dialogues for "He_RageArts00_St"
      {
        addr = moveset.getMoveAddress(0x942c4d5c, defaultAliasIdx - 20); // He_RageArts01_St
        addr = moveset.getMoveExtrapropAddr(addr);
        while (true)
        {
          int frame = moveset.getExtrapropValue(addr, "frame");
          int prop = moveset.getExtrapropValue(addr, "prop");
          if (frame == 0 && prop == 0) break;
          int reqIdx = moveset.getExtrapropValue(addr, "requirement_idx");
          if (reqIdx != 0)
          {
            moveset.editExtrapropValue(addr, "prop", 0);
          }
          addr = moveset.iterateExtraprops(addr, 1);
        }
      }

      // Enabling Shadow Dialogues for "He_RageArts01_St"
      addr = moveset.getMoveAddress(0xfb78fa92, defaultAliasIdx - 20); // He_RageArts01_St
      addr = moveset.getMoveExtrapropAddr(addr);
      addr = moveset.findExtraProp(addr, ExtraMoveProperties::STORE_VALUE_80C5);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));

      // TODO: b,f+2 functional
      // TODO: Broken Toy functional
      // Nice to have: d/f+1, 1 using the old animation
      return markMovesetEdited(movesetAddr);
    }

    if (bossCode == BossCodes::AmnesiaHeihachi)
    {
      // Disabling WI completely
      // {
      //   addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      //   addr = moveset.getMoveExtrapropAddr(addr);
      //   addr = moveset.findExtraProp(addr, ExtraMoveProperties::_0x8555);
      //   if (addr != 0)
      //   {
      //     moveset.editExtrapropValue(addr, "requirement_idx", 0);
      //     moveset.editExtrapropValue(addr, "prop", ExtraMoveProperties::HEI_WARRIOR);
      //     moveset.editExtrapropValue(addr, "value", 0);
      //   }
      // }

      // Adjusting Heat Smash
      {
        addr = moveset.getMoveAddress(0xc9c8dd57, idleStanceIdx); // He_ZoneD
        addr = moveset.getMoveNthCancel(addr, 1); // 2nd cancel
        moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
        addr = moveset.iterateCancel(addr, 1); // 3rd cancel
        moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      }

      // 2nd hit of regular 2,2
      addr = moveset.getMoveAddress(0xf69e2bef, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::DLC_STORY1_FLAGS, 1);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      // Alternate 2nd hit of 2,2
      addr = moveset.getMoveAddress(0xaffba07b, defaultAliasIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::DLC_STORY1_FLAGS, 1);
      for (int i = 0; i < 4; i++) // 4 cancels have the req that need to be disabled
      {
        uintptr_t reqs = moveset.getCancelValue(moveset.iterateCancel(addr, i), "requirements");
        moveset.disableStoryRelatedReqs(reqs);
      }
      return markMovesetEdited(movesetAddr);
    }

    if (bossCode == BossCodes::FinalHeihachi)
    {
      // Health regenration prop
      // {
      //   addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      //   addr = moveset.getMoveExtrapropAddr(addr);
      //   addr = moveset.findExtraProp(addr, ExtraMoveProperties::_0x8555);
      //   for (int i = 0; i < 2; i++)
      //   {
      //     uintptr_t reqAddr = moveset.getCancelValue(addr, "requirements");
      //     moveset.editRequirement(moveset.iterateRequirements(reqAddr, 0), 0, 0); // 1st req
      //     moveset.editRequirement(moveset.iterateRequirements(reqAddr, 1), 0, 0); // 2nd req
      //     addr = moveset.iterateExtraprops(addr, 1);
      //   }
      // }

      // Activating Heat on idle stance
      {
        // He_sKAM00_shadow
        uintptr_t addr = moveset.getMoveAddress(0xb36a0b80, 0x8000);
        addr = moveset.getMoveNthCancel(addr, 0);
        uintptr_t reqList = moveset.getCancelValue(addr, "requirements");
        // Preparing req-list
        addr = moveset.editRequirement(reqList, Requirements::HEAT_AVAILABLE, 1, 0);
        addr = moveset.editRequirement(addr, Requirements::PLAYER_IN_HEAT, 0, 0);
        addr = moveset.editRequirement(addr, Requirements::CHECK_TRADE, 0, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEAT_ENGAGER_SUCCESS_FLAG, 1, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEI_WARRIOR, 1, 0);
        addr = moveset.editRequirement(addr, 0x83f4, 1, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::MULTILEVEL_INSTALLS, 3, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEAT_METER, 1, 1);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::ADD_HEAT_VALUE, 900, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEAT_RELATED, 300, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEAT_AURA_VFX, 0, 0);
        addr = moveset.editRequirement(addr, Requirements::EOL, 0, 0);
        // Assigning this reqList to idle stance
        uintptr_t cancel = moveset.getMoveNthCancel(moveset.getMoveAddrByIdx(0x8001));
        moveset.editCancelValue(cancel, "requirements", reqList);
        moveset.editCancelValue(cancel, "extradata", moveset.findCancelExtradata(10240));
        moveset.editCancelValue(cancel, "start", 1);
        moveset.editCancelValue(cancel, "end", 32767);
        moveset.editCancelValue(cancel, "transition", 1);
        moveset.editCancelValue(cancel, "move", 0x8001);
        moveset.editCancelValue(cancel, "option", 257);
      }

      // Activating WI in intro against Lidia
      {
        addr = moveset.getMoveAddress(0xE323DEDC, defaultAliasIdx);
        addr = moveset.getMoveExtrapropAddr(addr);
        addr = moveset.findExtraProp(addr, ExtraMoveProperties::HEI_WARRIOR);
        uintptr_t reqAddr = moveset.getExtrapropValue(addr, "requirements");
        // Iterating all extraprops to disable all props that have a requirement addr
        while (true)
        {
          TK_ExtraProp prop = moveset.getExtraProp(addr);
          if (prop.requirements_ptr == reqAddr)
          {
            moveset.editExtrapropValue(addr, "requirement_idx", 0);
          }
          if (prop.property == 0 && prop.frame == 0)
            break;
          addr = moveset.iterateExtraprops(addr, 1);
        }
      }

      // Enable most of the moves by modifying 2,2
      addr = moveset.getMoveAddress(0xF69E2BEF, 1550);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      int new22 = moveset.getCancelMoveId(addr);
      uintptr_t moveAddr = moveset.getMoveAddrByIdx(new22);
      // 2,2,2
      addr = moveset.getMoveNthCancel(moveAddr, 5);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      addr = moveset.getMoveNthCancel(moveAddr, 6);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      addr = moveset.getMoveNthCancel(moveAddr, 7);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      addr = moveset.getMoveNthCancel(moveAddr, 8);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      // 1,1 > 1+3 throw
      addr = moveset.getMoveAddress(0x10E04C8A, 2000);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.disableStoryRelatedReqs(moveset.getCancelValue(addr, "requirements"));
      // Parry cancels from idle stance
      addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::DLC_STORY1_FLAGS, 3);
      // 4-cancels for parries
      if (!shouldDisableAutoParries())
      {
        for (int i = 0; i < 4; i++)
        {
          moveset.disableStoryRelatedReqs(moveset.getCancelValue(moveset.iterateCancel(addr, i), "requirements"));
        }
      }

      int preRound1 = defaultAliasIdx - 3;
      int preRound2 = defaultAliasIdx - 2;
      {
        uintptr_t defaultAliasAddr = moveset.getMoveAddrByIdx(defaultAliasIdx);
        addr = moveset.findMoveCancelByCondition(defaultAliasAddr, Requirements::PRE_ROUND_ANIM, -1, 50);
        moveset.editCancelMoveId(addr, preRound1);
        moveset.editCancelMoveId(moveset.iterateCancel(addr, 1), preRound2);

        // Now enabling story reqs inside their props
        handleHeihachiMoveProp(movesetAddr, preRound1);
        handleHeihachiMoveProp(movesetAddr, preRound2);
      }
      return markMovesetEdited(movesetAddr);
    }

    return false;
  }

  bool loadTrueDevilKazuya(uintptr_t movesetAddr, int bossCode)
  {
    if (bossCode != BossCodes::TrueDevilKazuya)
      return false;
    TkMoveset moveset(this->game, movesetAddr, this->decryptFuncAddr);
    uintptr_t addr = 0;
    try {
      addr = moveset.getMoveAddress(0xc8c48167);
    } catch (...) {
      return false;
    }
    addr = moveset.getMoveNthCancel(addr);
    addr = moveset.findCancelByCondition(addr, Requirements::ARCADE_BATTLE);
    moveset.disableRequirement(moveset.getCancelValue(addr, "requirements"), Requirements::ARCADE_BATTLE);
    addr = moveset.iterateCancel(addr, 1); // Next cancel
    moveset.disableRequirement(moveset.getCancelValue(addr, "requirements"), Requirements::ARCADE_BATTLE);

    addr = moveset.getMoveAddress(0xfebdae71); // Kz_Direct
    addr = moveset.getMoveNthCancel(addr, 1);
    {
      int moveId = moveset.getCancelValue(addr, "move");
      if (moveId == moveset.getMoveId(0x69fa69b1)) // grl_s00
      {
        addr = moveset.getCancelValue(addr, "requirements");
        addr = moveset.editRequirement(addr, Requirements::INTRO_RELATED, 0);
        addr = moveset.editRequirement(addr, Requirements::EOL, 0);
      }
    }

    return markMovesetEdited(movesetAddr);
  }

  bool loadStoryDevilJin(uintptr_t movesetAddr, int bossCode)
  {
    if (!isValidDevilJinBoss(bossCode))
      return false;
    TkMoveset moveset(this->game, movesetAddr, this->decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    uintptr_t addr = 0;

    // Doesn't do anything
    // adjustIntroOutroReq(moveset, FighterId::DevilJin2, 2000); // I know targetReq is first seen after index 2000

    // Adjusting winposes
    {
      // This is no longer needed if you handle the cameras
      // int enderId = moveset.getMoveId(0xAB7FA036, defaultAliasIdx); // Grabbed ID of the match-ender
      // // Grabbing ID of the first intro from alias 0x8000
      // addr = moveset.getMoveAddrByIdx(defaultAliasIdx);
      // addr = moveset.getMoveNthCancel(addr, 1); // 2nd Cancel
      // int start = moveset.getCancelMoveId(addr);

      // uintptr_t cancel = 0;
      // addr = moveset.getMoveAddress(0xD9CDC1C0, start);
      // for (int i = 0; i < 3; i++)
      // {
      //   cancel = moveset.getMoveNthCancel(addr, 0);
      //   moveset.editCancelMoveId(cancel, enderId);
      //   addr += Sizes::Moveset::Move;
      // }

      // This had no affect
      // addr = moveset.getMoveAddress(0xAB7FA036); // Dj_Direct
      // addr = moveset.getMoveNthCancel(addr, 70);
      // while (true)
      // {
      //   bool flag1 = moveset.cancelHasCondition(addr, Requirements::FATE_RELATED1);
      //   bool flag2 = moveset.cancelHasCondition(addr, Requirements::FATE_RELATED2);
      //   if (flag1 || flag2)
      //   {
      //     // Setting requirement to "Story Mode" to effectively disable it
      //     uintptr_t reqAddr = moveset.getCancelValue(addr, "requirements");
      //     moveset.editRequirement(reqAddr, Requirements::STORY_BATTLE, 0);
      //   }
      //   if (moveset.getCancelValue(addr, "command") == 0x8000)
      //     break;
      //   addr = moveset.iterateCancel(addr, 1);
      // }

      addr = moveset.getMoveAddress(0xa02e070b, defaultAliasIdx - 20); // Dj_RageArts01
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.disableStoryRelatedReqs(moveset.getExtrapropValue(addr, "requirements"));

      addr = moveset.getMoveAddress(0xfe2cd621, defaultAliasIdx - 15); // Dj_RageArts_n
      // 1st extraprop
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.disableStoryRelatedReqs(moveset.getExtrapropValue(addr, "requirements"));
      // 5th extraprop
      addr = moveset.iterateExtraprops(addr, 4);
      moveset.disableStoryRelatedReqs(moveset.getExtrapropValue(addr, "requirements"));

      if (bossCode == BossCodes::DevilJin_2 || bossCode == BossCodes::DevilJin_3) {
        addr = moveset.findExtraProp(addr, ExtraMoveProperties::STORE_VALUE_80C8);
        moveset.disableStoryRelatedReqs(moveset.getExtrapropValue(addr, "requirements"));

        {
          uintptr_t start = moveset.getMovesetHeader("extra_move_properties");
          uintptr_t count = moveset.getMovesetCount("extra_move_properties");
          for (int i = 9000; i < count; i++) // 9000 idx is near the Rage Art
          {
            addr = start + (i * sizeof(TK_ExtraProp));
            int prop = moveset.getExtrapropValue(addr, "prop");
            int param = moveset.getExtrapropValue(addr, "value");
            if (prop == ExtraMoveProperties::RAGE_ART_CAMERA)
            {
              if (param == 48 || param == 49) {
                moveset.editExtraprop(addr, -1, param - 43);
              }
            }
          }
        }
      }

      // TEMP
      // {
      //   uintptr_t header, count;

      //   header = moveset.getMovesetHeader("requirements");
      //   count = moveset.getMovesetCount("requirements");

      //   for (uintptr_t i = 0; i < count; i++)
      //   {
      //     uintptr_t addr = header + i * sizeof(TK_Requirement);
      //     TK_Requirement requirement = moveset.getRequirement(addr);
      //     if (requirement.req == Requirements::STORY_BATTLE_NUM && requirement.param[0] == 0xC3)
      //     {
      //       moveset.editRequirement(addr, 0, 0);
      //     }
      //   }
      // }
    }

    return markMovesetEdited(movesetAddr);
  }

  void syncCameraRemoteState()
  {
    if (!cameraRemoteState)
      return;
    CameraTrainerState state = {
        static_cast<uint32_t>(bossCode_L),
        static_cast<uint32_t>(bossCode_R),
        0};
    // Preserve current eligible flag
    state.eligible = game.readUInt32(reinterpret_cast<uintptr_t>(cameraRemoteState) + 8);
    game.write(reinterpret_cast<uintptr_t>(cameraRemoteState), state);
  }

  void syncCameraEligible(bool eligible)
  {
    if (!cameraRemoteState)
      return;
    game.write<uint32_t>(reinterpret_cast<uintptr_t>(cameraRemoteState) + 8, eligible ? 1u : 0u);
  }

  std::vector<uint8_t> buildCameraShellcode(uintptr_t remoteStateAddr, uintptr_t returnAddr)
  {
    std::vector<uint8_t> code;
    auto emit = [&](std::initializer_list<uint8_t> bytes)
    {
      code.insert(code.end(), bytes);
    };
    auto emitU32 = [&](uint32_t value)
    {
      for (int i = 0; i < 4; ++i)
        code.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    };
    auto emitU64 = [&](uint64_t value)
    {
      for (int i = 0; i < 8; ++i)
        code.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    };
    // Emit opcode then 4-byte rel32 placeholder; returns offset of the rel32 operand
    auto emitRel32Hole = [&](std::initializer_list<uint8_t> opcode) -> size_t
    {
      emit(opcode);
      size_t at = code.size();
      emitU32(0);
      return at;
    };
    auto patchRel32 = [&](size_t hole, size_t target)
    {
      int32_t rel = static_cast<int32_t>(target) - static_cast<int32_t>(hole + 4);
      code[hole + 0] = static_cast<uint8_t>(rel & 0xFF);
      code[hole + 1] = static_cast<uint8_t>((rel >> 8) & 0xFF);
      code[hole + 2] = static_cast<uint8_t>((rel >> 16) & 0xFF);
      code[hole + 3] = static_cast<uint8_t>((rel >> 24) & 0xFF);
    };

    // push rax
    emit({0x50});
    // mov rax, remoteStateAddr
    emit({0x48, 0xB8});
    emitU64(remoteStateAddr);

    // cmp dword [rax+8], 0 / je skip
    emit({0x83, 0x78, 0x08, 0x00});
    size_t jeSkipEligible = emitRel32Hole({0x0F, 0x84});

    // cmp r9d, 6 / je check_cam (Jin)
    emit({0x41, 0x83, 0xF9, FighterId::Jin});
    size_t jeCheckCamJin = emitRel32Hole({0x0F, 0x84});
    // cmp r9d, 35 / je check_cam (Heihachi)
    emit({0x41, 0x83, 0xF9, FighterId::Heihachi});
    size_t jeCheckCamHei = emitRel32Hole({0x0F, 0x84});
    // cmp r9d, 121 / je check_cam (DevilJin2)
    emit({0x41, 0x83, 0xF9, FighterId::DevilJin2});
    size_t jeCheckCamDvj = emitRel32Hole({0x0F, 0x84});
    // jmp skip
    size_t jmpSkipChar = emitRel32Hole({0xE9});

    size_t checkCam = code.size();
    patchRel32(jeCheckCamJin, checkCam);
    patchRel32(jeCheckCamHei, checkCam);
    patchRel32(jeCheckCamDvj, checkCam);

    // cmp r8d, 0x24 / jb check_p2
    emit({0x41, 0x83, 0xF8, 0x24});
    size_t jbCheckP2 = emitRel32Hole({0x0F, 0x82});
    // cmp r8d, 0x29 / ja check_p2
    emit({0x41, 0x83, 0xF8, 0x29});
    size_t jaCheckP2 = emitRel32Hole({0x0F, 0x87});

    // --- P1: dispatch by character ---
    // cmp r9d, 6 / je p1_jin
    emit({0x41, 0x83, 0xF9, FighterId::Jin});
    size_t jeP1Jin = emitRel32Hole({0x0F, 0x84});
    // cmp r9d, 35 / je p1_hei
    emit({0x41, 0x83, 0xF9, FighterId::Heihachi});
    size_t jeP1Hei = emitRel32Hole({0x0F, 0x84});
    // cmp r9d, 121 / je p1_dvj
    emit({0x41, 0x83, 0xF9, FighterId::DevilJin2});
    size_t jeP1Dvj = emitRel32Hole({0x0F, 0x84});
    size_t jmpSkipP1Unknown = emitRel32Hole({0xE9});

    auto emitP1Compare = [&](uint32_t bossCode, size_t &jeRemapHole)
    {
      emit({0x81, 0x38});
      emitU32(bossCode);
      jeRemapHole = emitRel32Hole({0x0F, 0x84});
    };

    size_t p1Jin = code.size();
    patchRel32(jeP1Jin, p1Jin);
    size_t jeRemapP1Jin[5];
    emitP1Compare(BossCodes::NerfedJin, jeRemapP1Jin[0]);
    emitP1Compare(BossCodes::MishimaJin, jeRemapP1Jin[1]);
    emitP1Compare(BossCodes::KazamaJin, jeRemapP1Jin[2]);
    emitP1Compare(BossCodes::FinalJin, jeRemapP1Jin[3]);
    emitP1Compare(BossCodes::ChainedJin, jeRemapP1Jin[4]);
    size_t jmpSkipP1Jin = emitRel32Hole({0xE9});

    size_t p1Hei = code.size();
    patchRel32(jeP1Hei, p1Hei);
    size_t jeRemapP1Hei0, jeRemapP1Hei1;
    emitP1Compare(BossCodes::AmnesiaHeihachi, jeRemapP1Hei0);
    emitP1Compare(BossCodes::ShadowHeihachi, jeRemapP1Hei1);
    size_t jmpSkipP1Hei = emitRel32Hole({0xE9});

    size_t p1Dvj = code.size();
    patchRel32(jeP1Dvj, p1Dvj);
    size_t jeRemapP1Dvj0, jeRemapP1Dvj1;
    emitP1Compare(BossCodes::DevilJin_2, jeRemapP1Dvj0);
    emitP1Compare(BossCodes::DevilJin_3, jeRemapP1Dvj1);
    size_t jmpSkipP1Dvj = emitRel32Hole({0xE9});

    size_t checkP2 = code.size();
    patchRel32(jbCheckP2, checkP2);
    patchRel32(jaCheckP2, checkP2);

    // cmp r8d, 0x2A / jb skip
    emit({0x41, 0x83, 0xF8, 0x2A});
    size_t jbSkipP2Lo = emitRel32Hole({0x0F, 0x82});
    // cmp r8d, 0x2F / ja skip
    emit({0x41, 0x83, 0xF8, 0x2F});
    size_t jaSkipP2Hi = emitRel32Hole({0x0F, 0x87});

    // --- P2: dispatch by character ---
    emit({0x41, 0x83, 0xF9, FighterId::Jin});
    size_t jeP2Jin = emitRel32Hole({0x0F, 0x84});
    emit({0x41, 0x83, 0xF9, FighterId::Heihachi});
    size_t jeP2Hei = emitRel32Hole({0x0F, 0x84});
    emit({0x41, 0x83, 0xF9, FighterId::DevilJin2});
    size_t jeP2Dvj = emitRel32Hole({0x0F, 0x84});
    size_t jmpSkipP2Unknown = emitRel32Hole({0xE9});

    auto emitP2Compare = [&](uint32_t bossCode, size_t &jeRemapHole)
    {
      emit({0x81, 0x78, 0x04});
      emitU32(bossCode);
      jeRemapHole = emitRel32Hole({0x0F, 0x84});
    };

    size_t p2Jin = code.size();
    patchRel32(jeP2Jin, p2Jin);
    size_t jeRemapP2Jin[5];
    emitP2Compare(BossCodes::NerfedJin, jeRemapP2Jin[0]);
    emitP2Compare(BossCodes::MishimaJin, jeRemapP2Jin[1]);
    emitP2Compare(BossCodes::KazamaJin, jeRemapP2Jin[2]);
    emitP2Compare(BossCodes::FinalJin, jeRemapP2Jin[3]);
    emitP2Compare(BossCodes::ChainedJin, jeRemapP2Jin[4]);
    size_t jmpSkipP2Jin = emitRel32Hole({0xE9});

    size_t p2Hei = code.size();
    patchRel32(jeP2Hei, p2Hei);
    size_t jeRemapP2Hei0, jeRemapP2Hei1;
    emitP2Compare(BossCodes::AmnesiaHeihachi, jeRemapP2Hei0);
    emitP2Compare(BossCodes::ShadowHeihachi, jeRemapP2Hei1);
    size_t jmpSkipP2Hei = emitRel32Hole({0xE9});

    size_t p2Dvj = code.size();
    patchRel32(jeP2Dvj, p2Dvj);
    size_t jeRemapP2Dvj0, jeRemapP2Dvj1;
    emitP2Compare(BossCodes::DevilJin_2, jeRemapP2Dvj0);
    emitP2Compare(BossCodes::DevilJin_3, jeRemapP2Dvj1);
    size_t jmpSkipP2Dvj = emitRel32Hole({0xE9});

    size_t remap = code.size();
    emit({0x41, 0x81, 0xC0});
    emitU32(CAMERA_ID_STORY_DELTA);

    size_t skip = code.size();
    patchRel32(jeSkipEligible, skip);
    patchRel32(jmpSkipChar, skip);
    patchRel32(jmpSkipP1Unknown, skip);
    patchRel32(jmpSkipP1Jin, skip);
    patchRel32(jmpSkipP1Hei, skip);
    patchRel32(jmpSkipP1Dvj, skip);
    patchRel32(jbSkipP2Lo, skip);
    patchRel32(jaSkipP2Hi, skip);
    patchRel32(jmpSkipP2Unknown, skip);
    patchRel32(jmpSkipP2Jin, skip);
    patchRel32(jmpSkipP2Hei, skip);
    patchRel32(jmpSkipP2Dvj, skip);
    for (size_t hole : jeRemapP1Jin)
      patchRel32(hole, remap);
    patchRel32(jeRemapP1Hei0, remap);
    patchRel32(jeRemapP1Hei1, remap);
    patchRel32(jeRemapP1Dvj0, remap);
    patchRel32(jeRemapP1Dvj1, remap);
    for (size_t hole : jeRemapP2Jin)
      patchRel32(hole, remap);
    patchRel32(jeRemapP2Hei0, remap);
    patchRel32(jeRemapP2Hei1, remap);
    patchRel32(jeRemapP2Dvj0, remap);
    patchRel32(jeRemapP2Dvj1, remap);

    // pop rax
    emit({0x58});
    // Stolen prologue bytes (executed after camera logic)
    emit({0x48, 0x89, 0x5C, 0x24, 0x08});
    emit({0x48, 0x89, 0x74, 0x24, 0x18});
    emit({0x55});
    emit({0x57});
    emit({0x41, 0x54});
    // Absolute jmp back to original+14 (push r14 ...)
    emit({0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});
    emitU64(returnAddr);

    return code;
  }

  bool installStoryCameraHook()
  {
    if (cameraHookInstalled)
      return true;
    if (!cameraHookAddr)
    {
      AppendLog("Story camera hook: address not scanned");
      return false;
    }

    uintptr_t hookAddr = cameraHookAddr;
    uint8_t currentBytes[CAMERA_HOOK_PATCH_SIZE] = {};
    if (!game.readBytes(hookAddr, currentBytes, CAMERA_HOOK_PATCH_SIZE))
    {
      AppendLog("Story camera hook: failed to read hook site");
      return false;
    }
    if (memcmp(currentBytes, cameraHookOriginal, CAMERA_HOOK_PATCH_SIZE) != 0)
    {
      AppendLog("Story camera hook: unexpected bytes at hook site (skipped)");
      return false;
    }

    cameraRemoteState = game.allocateInTarget<CameraTrainerState>(1);
    if (!cameraRemoteState)
    {
      AppendLog("Story camera hook: failed to allocate remote state");
      return false;
    }

    CameraTrainerState initialState = {
        static_cast<uint32_t>(bossCode_L),
        static_cast<uint32_t>(bossCode_R),
        0};
    if (!game.writeBytes(reinterpret_cast<uintptr_t>(cameraRemoteState), &initialState, sizeof(initialState)))
    {
      AppendLog("Story camera hook: failed to write remote state");
      game.freeInTarget(cameraRemoteState);
      cameraRemoteState = nullptr;
      return false;
    }

    uintptr_t returnAddr = hookAddr + CAMERA_HOOK_PATCH_SIZE;
    std::vector<uint8_t> shellcode = buildCameraShellcode(
        reinterpret_cast<uintptr_t>(cameraRemoteState), returnAddr);

    // Absolute jmp — cave may be allocated anywhere
    cameraCodeCave = game.allocateInTarget<uint8_t>(shellcode.size());
    if (!cameraCodeCave)
    {
      AppendLog("Story camera hook: failed to allocate code cave");
      game.freeInTarget(cameraRemoteState);
      cameraRemoteState = nullptr;
      return false;
    }

    uintptr_t caveAddr = reinterpret_cast<uintptr_t>(cameraCodeCave);
    if (!game.writeBytes(caveAddr, shellcode.data(), shellcode.size()))
    {
      AppendLog("Story camera hook: failed to write code cave");
      game.freeInTarget(cameraCodeCave);
      game.freeInTarget(cameraRemoteState);
      cameraCodeCave = nullptr;
      cameraRemoteState = nullptr;
      return false;
    }

    DWORD caveOldProtect = 0;
    if (!game.protectMemory(caveAddr, shellcode.size(), PAGE_EXECUTE_READWRITE, &caveOldProtect))
    {
      AppendLog("Story camera hook: failed to protect code cave");
      game.freeInTarget(cameraCodeCave);
      game.freeInTarget(cameraRemoteState);
      cameraCodeCave = nullptr;
      cameraRemoteState = nullptr;
      return false;
    }

    // Patch: jmp qword ptr [rip+0]; dq caveAddr
    uint8_t patch[CAMERA_HOOK_PATCH_SIZE] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 8; ++i)
      patch[6 + i] = static_cast<uint8_t>((caveAddr >> (8 * i)) & 0xFF);

    DWORD hookOldProtect = 0;
    if (!game.protectMemory(hookAddr, CAMERA_HOOK_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &hookOldProtect))
    {
      AppendLog("Story camera hook: failed to unprotect hook site");
      game.freeInTarget(cameraCodeCave);
      game.freeInTarget(cameraRemoteState);
      cameraCodeCave = nullptr;
      cameraRemoteState = nullptr;
      return false;
    }

    if (!game.writeBytes(hookAddr, patch, CAMERA_HOOK_PATCH_SIZE))
    {
      AppendLog("Story camera hook: failed to patch hook site");
      game.protectMemory(hookAddr, CAMERA_HOOK_PATCH_SIZE, hookOldProtect, &hookOldProtect);
      game.freeInTarget(cameraCodeCave);
      game.freeInTarget(cameraRemoteState);
      cameraCodeCave = nullptr;
      cameraRemoteState = nullptr;
      return false;
    }

    game.protectMemory(hookAddr, CAMERA_HOOK_PATCH_SIZE, hookOldProtect, &hookOldProtect);
    cameraHookInstalled = true;
    AppendLog("Story camera hook installed (cave=0x%llX)", (unsigned long long)caveAddr);
    return true;
  }

  bool installDramaCameraHook()
  {
    if (dramaCameraHookInstalled)
      return true;
    if (!dramaCameraHookAddr)
    {
      AppendLog("Drama camera hook: address not scanned");
      return false;
    }

    uintptr_t hookAddr = dramaCameraHookAddr;
    uint8_t currentBytes[DRAMA_CAMERA_HOOK_PATCH_SIZE] = {};
    if (!game.readBytes(hookAddr, currentBytes, DRAMA_CAMERA_HOOK_PATCH_SIZE))
    {
      AppendLog("Drama camera hook: failed to read hook site");
      return false;
    }
    if (memcmp(currentBytes, dramaCameraHookOriginal, DRAMA_CAMERA_HOOK_PATCH_SIZE) != 0)
    {
      AppendLog("Drama camera hook: unexpected bytes at hook site (skipped)");
      return false;
    }

    uintptr_t returnAddr = hookAddr + DRAMA_CAMERA_HOOK_PATCH_SIZE;

    // cmp r9d, 121 / jne code / mov r9d, 12 / stolen prologue / abs jmp return
    std::vector<uint8_t> shellcode;
    auto emit = [&](std::initializer_list<uint8_t> bytes)
    {
      shellcode.insert(shellcode.end(), bytes);
    };
    auto emitU32 = [&](uint32_t value)
    {
      for (int i = 0; i < 4; ++i)
        shellcode.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    };
    auto emitU64 = [&](uint64_t value)
    {
      for (int i = 0; i < 8; ++i)
        shellcode.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    };

    // cmp r9d, 121
    emit({0x41, 0x83, 0xF9, 0x79});
    // jne code (rel8 = +6, skip mov r9d, 12)
    emit({0x75, 0x06});
    // mov r9d, 12
    emit({0x41, 0xB9});
    emitU32(12);
    // stolen prologue
    emit({0x48, 0x89, 0x5C, 0x24, 0x08});
    emit({0x55});
    emit({0x56});
    emit({0x57});
    emit({0x41, 0x54});
    emit({0x41, 0x55});
    emit({0x41, 0x56});
    // jmp qword ptr [rip+0]; dq returnAddr
    emit({0xFF, 0x25, 0x00, 0x00, 0x00, 0x00});
    emitU64(returnAddr);

    dramaCameraCodeCave = game.allocateInTarget<uint8_t>(shellcode.size());
    if (!dramaCameraCodeCave)
    {
      AppendLog("Drama camera hook: failed to allocate code cave");
      return false;
    }

    uintptr_t caveAddr = reinterpret_cast<uintptr_t>(dramaCameraCodeCave);
    if (!game.writeBytes(caveAddr, shellcode.data(), shellcode.size()))
    {
      AppendLog("Drama camera hook: failed to write code cave");
      game.freeInTarget(dramaCameraCodeCave);
      dramaCameraCodeCave = nullptr;
      return false;
    }

    DWORD caveOldProtect = 0;
    if (!game.protectMemory(caveAddr, shellcode.size(), PAGE_EXECUTE_READWRITE, &caveOldProtect))
    {
      AppendLog("Drama camera hook: failed to protect code cave");
      game.freeInTarget(dramaCameraCodeCave);
      dramaCameraCodeCave = nullptr;
      return false;
    }

    uint8_t patch[DRAMA_CAMERA_HOOK_PATCH_SIZE] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 8; ++i)
      patch[6 + i] = static_cast<uint8_t>((caveAddr >> (8 * i)) & 0xFF);

    DWORD hookOldProtect = 0;
    if (!game.protectMemory(hookAddr, DRAMA_CAMERA_HOOK_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &hookOldProtect))
    {
      AppendLog("Drama camera hook: failed to unprotect hook site");
      game.freeInTarget(dramaCameraCodeCave);
      dramaCameraCodeCave = nullptr;
      return false;
    }

    if (!game.writeBytes(hookAddr, patch, DRAMA_CAMERA_HOOK_PATCH_SIZE))
    {
      AppendLog("Drama camera hook: failed to patch hook site");
      game.protectMemory(hookAddr, DRAMA_CAMERA_HOOK_PATCH_SIZE, hookOldProtect, &hookOldProtect);
      game.freeInTarget(dramaCameraCodeCave);
      dramaCameraCodeCave = nullptr;
      return false;
    }

    game.protectMemory(hookAddr, DRAMA_CAMERA_HOOK_PATCH_SIZE, hookOldProtect, &hookOldProtect);
    dramaCameraHookInstalled = true;
    AppendLog("Drama camera hook installed (cave=0x%llX)", (unsigned long long)caveAddr);
    return true;
  }

public:
  GameClass game;

  TkBossLoader(int bossCode_L = BossCodes::None, int bossCode_R = BossCodes::None)
  {
    this->bossCode_L = bossCode_L;
    this->bossCode_R = bossCode_R;
    this->attached = false;
    this->config = nullptr;
  }

  TkBossLoader(GameClass &game)
  {
    this->game = game;
    this->bossCode_L = BossCodes::None;
    this->bossCode_R = BossCodes::None;
    this->attached = false;
    this->config = nullptr;
  }

  ~TkBossLoader()
  {
    uninstallStoryCameraHook();
    uninstallDramaCameraHook();
    restoreHudAddr(0);
  }

  // Explicit cleanup for Ctrl+C / GUI close (also invoked by destructor)
  void uninstallStoryCameraHook()
  {
    if (!cameraHookInstalled)
      return;

    uintptr_t hookAddr = cameraHookAddr;
    if (hookAddr)
    {
      DWORD oldProtect = 0;
      if (game.protectMemory(hookAddr, CAMERA_HOOK_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect))
      {
        game.writeBytes(hookAddr, cameraHookOriginal, CAMERA_HOOK_PATCH_SIZE);
        game.protectMemory(hookAddr, CAMERA_HOOK_PATCH_SIZE, oldProtect, &oldProtect);
      }
    }

    if (cameraCodeCave)
    {
      game.freeInTarget(cameraCodeCave);
      cameraCodeCave = nullptr;
    }
    if (cameraRemoteState)
    {
      game.freeInTarget(cameraRemoteState);
      cameraRemoteState = nullptr;
    }
    cameraHookInstalled = false;
  }

  void uninstallDramaCameraHook()
  {
    if (!dramaCameraHookInstalled)
      return;

    uintptr_t hookAddr = dramaCameraHookAddr;
    if (hookAddr)
    {
      DWORD oldProtect = 0;
      if (game.protectMemory(hookAddr, DRAMA_CAMERA_HOOK_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect))
      {
        game.writeBytes(hookAddr, dramaCameraHookOriginal, DRAMA_CAMERA_HOOK_PATCH_SIZE);
        game.protectMemory(hookAddr, DRAMA_CAMERA_HOOK_PATCH_SIZE, oldProtect, &oldProtect);
      }
    }

    if (dramaCameraCodeCave)
    {
      game.freeInTarget(dramaCameraCodeCave);
      dramaCameraCodeCave = nullptr;
    }
    dramaCameraHookInstalled = false;
  }

  void setDevModeFlag(bool flag)
  {
    this->devMode = flag;
  }

  void setConfig(ConfigFlags *config)
  {
    this->config = config;
  }

  void setHudAndCostumesFlag(bool flag)
  {
    if (this->config)
      this->config->handleHudAndCostumes = flag;
  }

  void attachToLogBox(HWND hwndLogBox)
  {
    this->hwndLogBox = hwndLogBox;
  }
  bool attach()
  {
    if (!this->attached)
    {
      bool attached = game.Attach(L"Polaris-Win64-Shipping.exe");
      this->attached = attached;
    }
    return this->attached;
  }
  // Getters
  int getBossCode_L()
  {
    return this->bossCode_L;
  }
  int getBossCode_R()
  {
    return this->bossCode_R;
  }
  bool isReady()
  {
    return this->ready;
  }
  // Setters
  void setBossCode_L(int code)
  {
    this->bossCode_L = code;
    syncCameraRemoteState();
  }
  void setBossCode_R(int code)
  {
    this->bossCode_R = code;
    syncCameraRemoteState();
  }
  void setBossCodes(int codeL, int codeR)
  {
    this->bossCode_L = codeL;
    this->bossCode_R = codeR;
    syncCameraRemoteState();
  }
  void setBossCodeForSelectedSide(int selectedSide, int bossCode)
  {
    (selectedSide != 0) ? this->bossCode_R = bossCode : this->bossCode_L = bossCode;
    syncCameraRemoteState();
  }
  void scanForAddresses()
  {
    scanAddresses();
  }

  // Utility methods
  void bossLoadMainLoop(int selectedSide = -1)
  {
    if (!this->attached)
      return;

    if (INSTALL_CAMERA_HOOKS)
    {
      installStoryCameraHook(); // best-effort; failure must not block boss loading
      installDramaCameraHook(); // best-effort; failure must not block boss loading
    }

    const std::vector<DWORD> offsets = {(DWORD)matchStructOffset, 0x50, 0x8, 0x18, 0x8};
    uintptr_t matchStructAddr = game.getAddress(offsets);
    if (!matchStructAddr)
    {
      this->attached = false;
      printf("Cannot find the match structure address.\n");
      return;
    }

    while (this->attached)
    {
      // Main Loop
      Sleep(10);

      if (this->bossCode_L == BossCodes::None && this->bossCode_R == BossCodes::None)
        continue;

      matchStructAddr = game.getAddress(offsets);
      if (matchStructAddr == 0)
      {
        syncCameraEligible(false);
        continue;
      }

      if (handleIcons)
      {
        modifyHudAddr(matchStructAddr);
      }

      if (!isEligible(matchStructAddr))
      {
        syncCameraEligible(false);
        continue;
      }

      syncCameraEligible(isEligible__ExcludeStory(matchStructAddr));

      if (selectedSide != -1)
      {
        loadCharacter(matchStructAddr, selectedSide, getCode(selectedSide));
      }
      else
      {
        loadCharacter(matchStructAddr, 0, this->bossCode_L);
        loadCharacter(matchStructAddr, 1, this->bossCode_R);
      }

      if (handleIcons)
      {
        hudHandler(matchStructAddr);
      }

      uintptr_t playerAddr = getPlayerAddress(0);
      if (playerAddr == 0)
      {
        if (selectedSide != -1)
        {
          costumeHandler(matchStructAddr, selectedSide, getCode(selectedSide));
        }
        else
        {
          costumeHandler(matchStructAddr, 0, this->bossCode_L);
          costumeHandler(matchStructAddr, 1, this->bossCode_R);
        }
        continue;
      }

      uintptr_t movesetAddr = getMovesetAddress(playerAddr);
      if (movesetAddr == 0)
      {
        continue;
      }

      if (handleIcons)
      {
        restoreHudAddr(matchStructAddr);
      }

      if (!movesetExists(movesetAddr))
      {
        continue;
      }

      if (selectedSide != -1)
      {
        int code = getCode(selectedSide);
        if (loadBoss(code, selectedSide))
        {
          AppendLog("Loaded Boss \"%s\" for Player %d", getBossName(code).c_str(), selectedSide + 1);
        }
      }
      else
      {
        if (loadBoss(this->bossCode_L, 0))
        {
          AppendLog("Loaded Boss \"%s\" for Player 1", getBossName(this->bossCode_L).c_str());
        }
        if (loadBoss(this->bossCode_R, 1))
        {
          AppendLog("Loaded Boss \"%s\" for Player 2", getBossName(this->bossCode_R).c_str());
        }
      }

      // if (devMode) break;
    }
  }

  // Code = Boss Code, Side = P1 or P2
  bool loadBoss(int bossCode, int side)
  {
    if (!this->attached)
      return false;
    if (bossCode == BossCodes::None)
      return false;
    uintptr_t playerAddr = getPlayerAddress(side);
    uintptr_t movesetAddr = getMovesetAddress(playerAddr);

    // Special-case: Enable/Disable Heihachi WI flag from P1 struct
    if (bossCode == BossCodes::AmnesiaHeihachi || bossCode == BossCodes::FinalHeihachi)
    {
      int value = bossCode == BossCodes::AmnesiaHeihachi ? 2 : 1;
      setHeihachiPermaWI(playerAddr, value);
    }

    if (isMovesetEdited(movesetAddr))
      return false;

    try // Added so if "getMoveAddress" throws an error, the whole trainer doesn't crash.
    {
      int charId = getCharId(playerAddr);
      switch (charId)
      {
      case FighterId::Jin:
        return loadJin(movesetAddr, bossCode);
      case FighterId::Kazuya:
      {
        if (bossCode == BossCodes::DevilKazuya)
        {
          setKazuyaPermaDevil(playerAddr, 1);
        }
        return loadKazuya(movesetAddr, bossCode);
      }
      case FighterId::Azazel:
        return loadAzazel(movesetAddr, bossCode);
      case FighterId::Heihachi:
        return loadHeihachi(movesetAddr, bossCode);
      case FighterId::AngelJin:
        return loadAngelJin(movesetAddr, bossCode);
      case FighterId::TrueDevilKazuya:
      {
        if (bossCode == BossCodes::TrueDevilKazuya)
        {
          setKazuyaPermaDevil(playerAddr, 1);
        }
        return loadTrueDevilKazuya(movesetAddr, bossCode);
      }
      case FighterId::DevilJin2:
        return loadStoryDevilJin(movesetAddr, bossCode);
      default:
        return false;
      }
    }
    catch (...)
    {
      return false;
    }
  }
};

bool isCorrectCharacter(int bossCode, int charId)
{
  switch (bossCode)
  {
  case BossCodes::RegularJin:
  case BossCodes::NerfedJin:
  case BossCodes::MishimaJin:
  case BossCodes::KazamaJin:
  case BossCodes::FinalJin:
  case BossCodes::ChainedJin:
    return charId == FighterId::Jin;
  case BossCodes::DevilKazuya:
  case BossCodes::FinalKazuya:
    return charId == FighterId::Kazuya;
  case BossCodes::FinalHeihachi:
  case BossCodes::ShadowHeihachi:
  case BossCodes::AmnesiaHeihachi:
    return charId == FighterId::Heihachi;
  default:
    return true;
  }
}

bool isValidJinBoss(int bossCode)
{
  return bossCode == BossCodes::RegularJin ||
         bossCode == BossCodes::NerfedJin ||
         bossCode == BossCodes::MishimaJin ||
         bossCode == BossCodes::KazamaJin ||
         bossCode == BossCodes::FinalJin ||
         bossCode == BossCodes::ChainedJin;
}

bool isValidDevilJinBoss(int bossCode)
{
  return bossCode == BossCodes::DevilJin ||
         bossCode == BossCodes::DevilJin_1 ||
         bossCode == BossCodes::DevilJin_2 ||
         bossCode == BossCodes::DevilJin_3;
}

bool isValidKazuyaBoss(int bossCode)
{
  return bossCode == BossCodes::DevilKazuya ||
         bossCode == BossCodes::FinalKazuya;
}

bool isValidHeihachiBoss(int bossCode)
{
  return bossCode == BossCodes::FinalHeihachi ||
         bossCode == BossCodes::ShadowHeihachi ||
         bossCode == BossCodes::AmnesiaHeihachi;
}

bool isStoryCameraBoss(int bossCode)
{
  if (isValidJinBoss(bossCode) && bossCode != BossCodes::RegularJin)
    return true;
  return bossCode == BossCodes::AmnesiaHeihachi ||
         bossCode == BossCodes::ShadowHeihachi ||
         bossCode == BossCodes::DevilJin_2 ||
         bossCode == BossCodes::DevilJin_3;
}

bool isCorrectHeihachiFlag(int storyFlag, int param)
{
  switch (storyFlag)
  {
  case 1:
    return (param >= 0x501 && param < 0x601);
  case 2:
    return (param >= 0x601 && param < 0x701);
  case 3:
    return (param >= 0x801);
  default:
    break;
  }
  return false;
}
