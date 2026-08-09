// This class will be responsible for loading boss characters
#include "moveset.h"
#include "utils.h"
#include <cstring>
#include <vector>

using namespace Tekken;

std::string FINAL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_naked_belt_off.CS_ant_1p_naked_belt_off";
std::string CHAINED_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_chain.CS_ant_1p_chain";
std::string FINAL_KAZ_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_grl_1p_v2_white.CS_grl_1p_v2_white";
std::string DEVIL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_swl_ant_1p.CS_swl_ant_1p";
std::string DEVIL_JIN_COSTUME_PATH_2 = "/Game/Demo/Story/Sets/CS_swl_ant_1p_horn.CS_swl_ant_1p_horn";
std::string DEVIL_JIN_COSTUME_PATH_3 = "/Game/Demo/Story/Sets/CS_swl_ant_1p_horn_bw.CS_swl_ant_1p_horn_bw";
std::string HEIHACHI_MONK_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_whitetiger_nohat_nomask.CS_bee_whitetiger_nohat_nomask";
std::string HEIHACHI_SHADOW_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_1p_p_shadow.CS_bee_1p_p_shadow";
bool ADJUST_RA_CAMERA = true;

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
  uintptr_t cameraHookAddr = 0;
  // STORY CAMERA HOOK
  CameraTrainerState *cameraRemoteState = nullptr;
  uint8_t *cameraCodeCave = nullptr;
  bool cameraHookInstalled = false;
  static constexpr size_t CAMERA_HOOK_PATCH_SIZE = 8;
  static constexpr uint32_t CAMERA_ID_STORY_DELTA = 0xDB;
  const uint8_t cameraHookOriginal[CAMERA_HOOK_PATCH_SIZE] = {
      0x48, 0x8D, 0xAC, 0x24, 0x50, 0xFE, 0xFF, 0xFF};
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

    // AoB starts at lea rbp,[rsp-1B0] — the Story RA camera hook injection point
    addr = game.FastAoBScan(Tekken::CAMERA_HOOK_SIG_BYTES, base + 0x5C00000);
    if (addr != 0)
    {
      cameraHookAddr = addr;
    }
    else
    {
      cameraHookAddr = 0;
      AppendLog("Story Camera Hook Address not found (camera remap disabled)");
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
    }
    this->ready = true; // Ready to load bosses
  }

  // Modifies the instructions that allows for custom HUD icon loading
  void modifyHudAddr(uintptr_t matchStructAddr)
  {
    int mode = game.readInt32(matchStructAddr);
    if (mode == 1 || mode == 6)
    {
      int icon = game.readUInt16(hudIconAddr);
      int name = game.readUInt16(hudNameAddr);
      if (icon == 0x5274 && name == 0x3174)
      {
        game.write<uint16_t>(hudIconAddr, 0x9090);
        game.write<uint16_t>(hudNameAddr, 0x9090);
      }
    }
  }

  void restoreHudAddr(uintptr_t matchStructAddr)
  {
    int icon = game.readUInt16(hudIconAddr);
    int name = game.readUInt16(hudNameAddr);
    if (icon == 0x9090 && name == 0x9090)
    {
      game.write<uint16_t>(hudIconAddr, 0x5274);
      game.write<uint16_t>(hudNameAddr, 0x3174);
    }
  }

  void loadBossHud(uintptr_t matchStruct, int side, int charId, int bossCode)
  {
    if (bossCode == BossCodes::None)
      return;
    std::string icon;
    std::string name;
    const char c = side == 0 ? 'L' : 'R';
    bool isStoryDvj = isValidDevilJinBoss(bossCode) && charId == FighterId::DevilJin2;
    if (isStoryDvj)
    {
      if (bossCode == BossCodes::DevilJin || bossCode == BossCodes::DevilJin_1) {
        icon = buildString(c, getCharCode(FighterId::Jin));
        name = getNamePath(FighterId::Jin);
      }
      else if (bossCode == BossCodes::DevilJin_2) {
        icon = buildString(c, HudIcon::DvjCh12);
        name = getNamePath(FighterId::DevilJin);
      }
      else if (bossCode == BossCodes::DevilJin_3) {
        icon = buildString(c, HudIcon::DvjCh13);
        name = getNamePath(FighterId::DevilJin);
      }
    } 
    else if ((bossCode == BossCodes::FinalJin || bossCode == BossCodes::MishimaJin || bossCode == BossCodes::KazamaJin) && charId == FighterId::Jin)
    {
      icon = buildString(c, HudIcon::JinFinal);
      name = getNamePath(FighterId::Jin);
    }
    else if (bossCode == BossCodes::FinalKazuya && charId == FighterId::Kazuya)
    {
      icon = buildString(c, HudIcon::KazFinal);
      name = getNamePath(FighterId::Kazuya);
    }
    else if (bossCode == BossCodes::DevilKazuya && charId == FighterId::Kazuya)
    {
      icon = buildString(c, HudIcon::KazDevil);
      name = getNamePath(HudName::KazDevil);
    }
    else if (bossCode == BossCodes::AmnesiaHeihachi && charId == FighterId::Heihachi)
    {
      icon = buildString(c, HudIcon::HeiMonk);
      name = getNamePath(FighterId::Heihachi);
    }
    else if (bossCode == BossCodes::ShadowHeihachi && charId == FighterId::Heihachi)
    {
      icon = buildString(c, HudIcon::HeiShadow);
      name = getNamePath(HudName::HeiShadow);
    }
    if (!icon.empty() && (shouldHandleHudAndCostumes() || isStoryDvj))
      game.writeString(matchStruct + 0x2C0 + side * 0x100, icon);
    if (!name.empty() && (shouldHandleHudAndCostumes() || isStoryDvj))
      game.writeString(matchStruct + 0x4C0 + side * 0x100, name);
  }

  void hudHandler(uintptr_t matchStruct)
  {
    int char1 = game.readInt32(matchStruct + 0x10);
    int char2 = game.readInt32(matchStruct + 0x94);
    std::string icon1 = getIconPath(0, char1);
    std::string icon2 = getIconPath(1, char2);
    std::string name1 = getNamePath(char1);
    std::string name2 = getNamePath(char2);
    game.writeString(matchStruct + 0x2C0, icon1);
    game.writeString(matchStruct + 0x3C0, icon2);
    game.writeString(matchStruct + 0x4C0, name1);
    game.writeString(matchStruct + 0x5C0, name2);

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

  void loadCostume(uintptr_t matchStructAddr, int side, int costumeId, std::string costumePath)
  {
    game.write<int>(matchStructAddr + 0x6F0 + side * 0x6760, costumeId);
    game.writeString(matchStructAddr + 0x13D78 + side * 0x100, costumePath);
  }

  void costumeHandler(uintptr_t matchStructAddr, int side, int bossCode)
  {
    if (!matchStructAddr)
      return;
    if (!isCorrectCharacter(bossCode, getCharId(matchStructAddr, side)))
      return;
    std::string costumePath;
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
      requirement = reqHeader + i * Sizes::Moveset::Requirement;
      req = game.readInt32(requirement);
      if (req == Requirements::INTRO_RELATED)
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
      uintptr_t reqList = game.readInt32(addr + Offsets::ExtraProp::RequirementAddr);
      if (!prop && !frame)
        break;
      if (prop == ExtraMoveProperties::SPEND_RAGE)
      {
        moveset.editExtraprop(addr, -1, -1, 0); // don't spend rage
      }
      if (prop == ExtraMoveProperties::HEAT_RELATED)
      {
        moveset.editExtraprop(addr, -1, -1, 300);
      }
      if (prop == ExtraMoveProperties::SHORT_FLAG)
      {
        moveset.editExtraprop(addr, -1, -1, 0x220001);
      }
      // Cancels & Props both have requirements at offset 0x8
      if (moveset.cancelHasCondition(addr, Requirements::DLC_STORY1_BATTLE_NUM))
      {
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      }
      addr += Sizes::Moveset::ExtraMoveProperty;
    }
  }

  bool loadJin(uintptr_t movesetAddr, int bossCode)
  {
    if (!isValidJinBoss(bossCode))
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    int _777param = bossCode == BossCodes::ChainedJin ? 1 : bossCode;
    moveset.replaceRequirements(Requirements::STORY_FLAGS, _777param);
    moveset.replaceRequirements(Requirements::NOT_STORY_MODE, 0, Requirements::STORY_FLAGS);

    // Adjusting Rage Art
    // uintptr_t rageArt = moveset.getMoveAddress(0x9BAE061E, 2100);
    // if (rageArt && bossCode != BossCodes::RegularJin)
    // {
    //   uintptr_t cancel = moveset.getMoveNthCancel(rageArt, 0);
    //   moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0x1ADAB0CB, 2000));
    // }
    // auto fixRageArtCamera = [&](uintptr_t startAddr) {
    //   for (uintptr_t addr = startAddr;;
    //        addr = moveset.iterateExtraprops(addr, 1)) {
    //     int prop = moveset.getExtrapropValue(addr, "prop");

    //     if (prop == ExtraMoveProperties::RAGE_ART_CAMERA)
    //       moveset.editExtrapropValue(addr, "value", 5);

    //     if (!prop && !moveset.getExtrapropValue(addr, "frame"))
    //       break;
    //   }
    // };

    // uintptr_t rageArt = moveset.getMoveAddress(0x9bae061e, 2300);
    // if (rageArt && bossCode != BossCodes::RegularJin)
    // {
    //   fixRageArtCamera(moveset.getMoveExtrapropAddr(rageArt));
    //   rageArt = moveset.getMoveAddress(0x22e4beeb, 2100);
    //   fixRageArtCamera(moveset.getMoveExtrapropAddr(rageArt));
    // }

    // Rage Art Camera (requires Assembly Injection)
    auto setRageArtCamera = [&](uint32_t nameKey, int value)
    {
      if (!ADJUST_RA_CAMERA) return;
      uintptr_t addr = moveset.getMoveAddress(nameKey, moveset.getAliasMoveId(0x8000) - 20);
      addr = moveset.getMoveExtrapropAddr(addr);
      addr = moveset.findExtraProp(addr, ExtraMoveProperties::RAGE_ART_CAMERA);

      if (addr)
      {
        moveset.editExtrapropValue(addr, "value", value);
      }
    };

    if (bossCode != BossCodes::RegularJin) {
      setRageArtCamera(0x9bae061e, 5); // Jz_Story_RageArts00
      setRageArtCamera(0x22e4beeb, 5); // Jz_RageArts01_St
      setRageArtCamera(0x5898e42a, 6); // Jz_RageArts_n_St
    }

    // Handling season 2 bugs and moves
    // FF+1+2 and FC df4 ~ ZEN cancel
    if (bossCode != BossCodes::RegularJin)
    {
      uintptr_t addr = 0;
      int moveId = -1;
      // Replace the new f,f+1+2 with f,f+2. Taking the long approach to remove weird animation snap
      {
        int ff2 = moveset.getMoveId(0xE383D012, 2200); // f,f+2
        int ff12 = moveset.getMoveId(0xEB242623, 1750); // f,f+1+2
        uintptr_t cancel = moveset.getMovesetHeader("cancels");
        uintptr_t count = moveset.getMovesetCount("cancels");
        for (int i = 3500; i < (count - 2000); i++) { // First cancel appears around index 3700 and last around 11000
          uintptr_t addr = cancel + i * Sizes::Moveset::Cancel;
          if (moveset.getCancelMoveId(addr) == ff12)
          {
            moveset.editCancelMoveId(addr, (short)ff2);
          }
        }
      }

      // Adjusting FC df4 ~ ZEN cancels
      addr = moveset.getMoveAddress(0x8c0f6a17, 1600); // Jz_zan_srk00EX_zan
      if (addr) {
        uintptr_t firstCancel = moveset.getMoveNthCancel(addr, 0);
        uintptr_t cancel = 0;

        // Replacing df from that ZEN with the story version
        moveId = moveset.getMoveId(0xda8608b7, 1790); // Jz_shoryu_P
        cancel = moveset.findCancel(firstCancel, "move", moveId);
        if (cancel) {
          moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0x39b5f537, 2200));
        }

        // ZEN 1+2 becomes ZEN u+1+2 because of command priority
        cancel = moveset.findCancel(firstCancel, "command", 0x4000000300000000);
        if (cancel) {
          moveset.editMoveCancel(cancel,
            0x4000000300000300,
            moveset.getMovesetHeader("requirements"),
            0,
            -1,
            -1,
            -1,
            (short)moveset.getMoveId(0x91130746, 2300), // ZEN u+1+2
            -1);
        }

        // Adjusting ZEN 3+4
        moveId = moveset.getMoveId(0x362078c4, 1820); // ZEN 3+4
        cancel = moveset.findCancel(firstCancel, "move", moveId);
        if (cancel) {
          moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0x1a53432b, 2300));
        }

        // Disabling ZEN u+1
        moveId = moveset.getMoveId(0xc69959b0, 1580); // ZEN u+1
        cancel = moveset.findCancel(firstCancel, "move", moveId);
        if (cancel) {
          moveset.editMoveCancel(
            cancel, 
            0x4000000300000000,
            0,
            0,
            -1,
            -1,
            -1,
            (short)moveset.getMoveId(0xb235481b, 1600), // ZEN 1+2
            -1);
        }

        // Adjusting ZEN 1
        moveId = moveset.getMoveId(0xea6240d3, 1580); // ZEN 1
        cancel = moveset.findCancel(firstCancel, "move", moveId);
        if (cancel) {
          moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0x69655f3c, 2300));
        }

        // Adjusting ZEN 2
        moveId = moveset.getMoveId(0xc48dd080, 1580); // ZEN 2
        cancel = moveset.findCancel(firstCancel, "move", moveId);
        if (cancel) {
          moveset.editCancelExtradata(cancel, moveset.findCancelExtradata(389));
          moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0xa34e66df, 2300));
        }

        // Adjusting ZEN 4
        moveId = moveset.getMoveId(0xfd3fe1a6, 1800); // ZEN 2
        cancel = moveset.findCancel(firstCancel, "move", moveId);
        if (cancel) {
          moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0xc2c9eadc, 2300));
        }
      }
    }

    // Solving the "ws+1, [3,3] ~ df" bug
    if (bossCode != BossCodes::MishimaJin)
    {
      uintptr_t addr = moveset.getMoveAddress(0x530890fb, moveset.getAliasMoveId(0x8000));
      addr = moveset.getMoveNthCancel(addr, 2);
      moveset.editCancelMoveId(addr, moveset.getMoveId(0x459c84c1, 1800)); // Jz_shoryu_shift
    }

    // EWGF > OTGF bug fix. Only Final Jin should be able to do it, other variants shouldn't
    if (bossCode != BossCodes::FinalJin)
    {
      uintptr_t addr = moveset.getMoveAddress(0x39b5f537, moveset.getAliasMoveId(0x8000));
      addr = moveset.getMoveNthCancel(addr, 0);
      addr = moveset.findCancel(addr, "command", 0x4000000300000000);
      int moveId = moveset.getCancelMoveId(moveset.iterateCancel(addr, 1)); // Get CD+1 ID from next cancel
      moveset.editCancelMoveId(addr, moveId); // Jz_Story_623_LP_fast
    }

    switch (bossCode)
    {
    case BossCodes::RegularJin:
    {
      uintptr_t addr = moveset.getMoveAddress(0x9b789d36, 1865); // d/b+1+2
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 0), 0);
    }
    break;
    case BossCodes::NerfedJin:
    case BossCodes::FinalJin:
    {
      // Disabling auto-parries
      if (BossCodes::FinalJin == bossCode && shouldDisableAutoParries()) {
        uintptr_t addr = moveset.getMoveAddrByIdx(0x8001);
        int targetMoveId = moveset.getMoveId(0xc2da6f70, 2500);
        uintptr_t cancel = moveset.findCancel(moveset.getMoveNthCancel(addr), "move", targetMoveId);
        if (cancel) {
          for (int i = 0; i < 4; i++) {
            uintptr_t reqAddr = moveset.getCancelReqAddr(cancel);
            moveset.editRequirement(reqAddr, Requirements::STORY_BATTLE);
            cancel = moveset.iterateCancel(cancel, 1);
          }
        }
      }
    }
      break;
    case BossCodes::MishimaJin:
    case BossCodes::KazamaJin:
    {
      uintptr_t moveId = moveset.getMoveId(bossCode == BossCodes::MishimaJin ? 0xA33CD19D : 0x7614EF15, 2000);
      if (moveId != 0)
      {
        game.write<short>(movesetAddr + 0xAA, moveId);
      }
    }
    break;
    case BossCodes::ChainedJin:
    {
      uintptr_t reqHeader = moveset.getMovesetHeader("requirements");
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
          moveset.editCancelReqAddr(moveset.getMoveNthCancel(moveAddr, 0), reqHeader);
        }
      }
    }
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

      // Disabling some requirements for basic attacks
      // 0x8000 alias
      // addr = movesHeader + (defaultAliasIdx * Sizes::Moveset::Move);
      // 32th cancel
      // disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 31), Requirements::STORY_FLAGS);

      addr = moveset.getMoveAddress(0x42CCE45A, idleStanceIdx); // CD+4, 1 last hit key
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE, -1);
      if (addr != 0)
      {
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
        addr = moveset.iterateCancel(addr, 2); // Move 2 cancels forward
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      }

      // 1,1,2
      addr = moveset.getMoveAddress(0x2226A9EE, idleStanceIdx);
      // 3rd cancel
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 2));

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
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      addr += Sizes::Moveset::Cancel; // Move 1 cancel forward
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));

      // d/b+1+2
      addr = moveset.getMoveAddress(0x73EBDBA2, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      moveset.editCancelReqAddr(addr, moveset.getMovesetHeader("requirements"));

      // d/b+4
      addr = moveset.getMoveAddress(0x9364E2F5, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      addr = moveset.getCancelReqAddr(addr);
      moveset.disableStoryRelatedReqs(addr);
      // Disabling standing req
      game.write<int>(addr + Sizes::Moveset::Requirement, 0);

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
        uintptr_t addr = start + (i * Sizes::Moveset::Requirement);
        int req = moveset.getRequirementValue(addr, "req");
        int param = moveset.getRequirementValue(addr, "param");
        if ((req == ExtraMoveProperties::DEVIL_STATE && param >= 1) || (req == ExtraMoveProperties::WING_ANIM))
        {
          moveset.editRequirement(addr, 0, 0);
        }
      }

      // extraprops
      start = moveset.getMovesetHeader("extra_move_properties");
      count = moveset.getMovesetCount("extra_move_properties");

      for (uintptr_t i = 2200; i < count; i++)
      {
        uintptr_t addr = start + (i * Sizes::Moveset::ExtraMoveProperty);
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
        moveset.editMoveCancel(
            addr,
            0,
            moveset.getMovesetHeader("requirements"),
            moveset.findCancelExtradata(1025),
            1,
            1,
            1,
            (short)moveId,
            65);
      }

      // Replacing Rage Art with Tekken-Ball counterpart
      if (config->finalKazuyaRageBlast) {
        // Kz_RageArts00
        uintptr_t addr = moveset.getMoveAddress(0xfaf65ab0, defaultAliasIdx - 100);
        int moveId = moveset.getMoveId(0xffd3c168, idleStanceIdx - 100); // Tekken Ball blast
        addr = moveset.getMoveNthCancel(addr, 0);
        moveset.editMoveCancel(
            addr,
            0,
            moveset.getMovesetHeader("requirements"),
            moveset.findCancelExtradata(1025),
            1,
            1,
            1,
            (short)moveId,
            65);
      }

      // Single-spin uppercut
      uintptr_t addr = moveset.getMoveAddress(0xD172C176, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelCommand(addr, 0x10);
      moveset.editCancelOption(addr, 0x50);

      uintptr_t reqHeader = moveset.getMovesetHeader("requirements");

      // Ultra-wavedash
      addr = moveset.getMoveAddress(0x77314B09, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelReqAddr(addr, reqHeader);

      // CD+1+2
      addr = moveset.getMoveAddress(0x0C9CE140, idleStanceIdx);
      moveset.editCancelReqAddr(moveset.getMoveNthCancel(addr, 0), reqHeader);

      // b+2,2
      addr = moveset.getMoveAddress(0x8B5BFA6C, idleStanceIdx);
      moveset.editCancelReqAddr(moveset.getMoveNthCancel(addr, 0), reqHeader);

      // NEW b+2,2. Disabling laser cancel
      addr = moveset.getMoveAddress(0x8FE28C6A, defaultAliasIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelExtradata(addr, moveset.findCancelExtradata(16383));

      // Disabling u/b+1+2 laser
      addr = moveset.getMoveAddress(0x07F32E0C, 2000);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editMoveCancel(
          addr,
          0,
          reqHeader,
          moveset.findCancelExtradata(386),
          -1,
          -1,
          1,
          moveset.getMoveId(0x1376C644, idleStanceIdx),
          65);

      // d/f+3+4, 1
      {
        int df34_1 = moveset.getMoveId(0x6562FA84, idleStanceIdx);
        addr = moveset.getMoveAddrByIdx(df34_1);
        addr = moveset.getMoveNthCancel(addr, 0);
        int df34_1_db2 = moveset.getCancelMoveId(addr);
        int df34_1_2 = moveset.getMoveId(0xD63CD0E6, df34_1);
        addr = moveset.findCancel(addr, "move", df34_1_2);
        moveset.editCancelMoveId(moveset.iterateCancel(addr, 0), df34_1_db2);
        moveset.editCancelMoveId(moveset.iterateCancel(addr, 1), df34_1_db2);
      }

      // d/b+1, 2
      addr = moveset.getMoveAddress(0xFE501006, idleStanceIdx); // d/b+1
      addr = moveset.getMoveNthCancel(addr, 0);
      // Grabbing move ID from 3rd cancel
      int moveId_db2 = moveset.getCancelMoveId(moveset.iterateCancel(addr, 2));
      addr = moveset.iterateCancel(addr, 9); // 10th cancel
      moveset.editCancelFrames(addr, 19, 19, 19);
      moveset.editCancelMoveId(addr, moveId_db2);

      // 11th cancel
      addr = moveset.iterateCancel(addr, 1);
      moveset.editCancelFrames(addr, 19, 19, 19);
      moveset.editCancelMoveId(addr, moveId_db2);

      // Adjusting d/b+1+2 to cancel into this on frame-1
      int moveId_db1 = moveset.getMoveId(0xFE501006, moveId_db2);
      addr = moveset.getMoveAddress(0x73EBDBA2, moveId_db1);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editCancelReqAddr(addr, reqHeader);
      moveset.editCancelMoveId(addr, (short)moveId_db1);

      // ws+2
      addr = moveset.getMoveAddress(0xB253E5F2, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editMoveCancel(
          addr,
          0,
          reqHeader,
          moveset.findCancelExtradata(1025),
          5,
          5,
          5,
          moveset.getMoveId(0x0AB42E52, defaultAliasIdx),
          65);

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
    addr = moveset.getMoveNthCancel1stReqAddr(addr, 0); // 1st req
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
    adjustIntroOutroReq(moveset, bossCode, 2085); // I know targetReq is first seen after index 2085

    // Fix Rage Art dialogue (2 cancels)
    uintptr_t addr = moveset.getMoveAddress(0x53089f24, moveset.getAliasMoveId(0x8000) - 25); // Rage Art

    uintptr_t cancelAddr = moveset.findMoveCancelByCondition(addr, Requirements::ARCADE_BATTLE);
    addr = moveset.findRequirement(moveset.getCancelReqAddr(cancelAddr), Requirements::ARCADE_BATTLE);
    moveset.editRequirement(addr, 0);

    cancelAddr = moveset.iterateCancel(cancelAddr, 1);
    addr = moveset.findRequirement(moveset.getCancelReqAddr(cancelAddr), Requirements::ARCADE_BATTLE);
    moveset.editRequirement(addr, 0);

    // Adjust damage for the new CD+1
    if (config->toneDownDamage) {
      addr = moveset.getMoveAddress(0x3cbbe67a, moveset.getAliasMoveId(0x8001));
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.editExtrapropValue(addr, "value", 0);
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

    if (bossCode == BossCodes::ShadowHeihachi)
    {
      addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      uintptr_t cancel1 = moveset.getMoveNthCancel(addr, 0);
      uintptr_t reqListCancel1 = moveset.getCancelReqAddr(cancel1);
      uintptr_t reqListCancel2 = moveset.getMoveNthCancel1stReqAddr(addr, 1);
      moveset.editCancelReqAddr(cancel1, reqListCancel2);
      moveset.disableStoryRelatedReqs(reqListCancel1);
      // TODO: b,f+2 functional
      // TODO: Broken Toy functional
      return markMovesetEdited(movesetAddr);
    }

    if (bossCode == BossCodes::AmnesiaHeihachi)
    {
      // Disabling WI completely
      // {
      //   addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      //   addr = moveset.findMoveExtraprop(addr, ExtraMoveProperties::_0x8555);
      //   if (addr != 0)
      //   {
      //     moveset.editExtrapropValue(addr, "requirements", moveset.getMovesetHeader("requirements"));
      //     moveset.editExtrapropValue(addr, "prop", ExtraMoveProperties::HEI_WARRIOR);
      //     moveset.editExtrapropValue(addr, "value", 0);
      //   }
      // }

      // Rage Art Camera (requires Assembly Injection)
      auto setRageArtCamera = [&](uint32_t nameKey, int value)
      {
        if (!ADJUST_RA_CAMERA) return;
        addr = moveset.getMoveAddress(nameKey, defaultAliasIdx - 20);
        addr = moveset.getMoveExtrapropAddr(addr);
        addr = moveset.findExtraProp(addr, ExtraMoveProperties::RAGE_ART_CAMERA);

        if (addr)
        {
          moveset.editExtrapropValue(addr, "value", value);
        }
      };

      setRageArtCamera(0xfb78fa92, 5); // He_RageArts01_St
      setRageArtCamera(0x140be639, 5); // He_RageArts02_St
      setRageArtCamera(0xa77873d3, 6); // He_RageArts_n_St

      // Adjusting RageArt against Kazuya & Jin & "friends"
      {
        int targetMoveId = moveset.getMoveId(0x942c4d5c);                // He_RageArts00_St
        addr = moveset.getMoveAddress(0xde97038f, defaultAliasIdx - 30); // He_RageArts00
        addr = moveset.getMoveNthCancel(addr, 0);
        moveset.editCancelMoveId(addr, (short)targetMoveId);
        moveset.editCancelReqAddr(addr, moveset.getMovesetHeader("requirements"));
        // printf("targetMoveId: %d\n", targetMoveId);
        // int i = 0;
        // while (true)
        // {
        //   int moveId = moveset.getCancelMoveId(addr);
        //   printf("[%d]: %d\n", i, moveId);
        //   if (moveId == targetMoveId)
        //   {
        //     break;
        //   }
        //   else
        //   {
        //     moveset.editCancelExtradata(addr, moveset.findCancelExtradata(16383));
        //   }
        //   addr = moveset.iterateCancel(addr, 1);
        //   i++;
        // }
      }

      // Adjusting Heat Smash
      {
        addr = moveset.getMoveAddress(0xc9c8dd57, idleStanceIdx); // He_ZoneD
        addr = moveset.getMoveNthCancel(addr, 1); // 2nd cancel
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
        addr = moveset.iterateCancel(addr, 1); // 3rd cancel
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      }

      // 2nd hit of regular 2,2
      addr = moveset.getMoveAddress(0xf69e2bef, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::DLC_STORY1_FLAGS, 1);
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      // Alternate 2nd hit of 2,2
      addr = moveset.getMoveAddress(0xaffba07b, defaultAliasIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::DLC_STORY1_FLAGS, 1);
      for (int i = 0; i < 4; i++) // 4 cancels have the req that need to be disabled
      {
        uintptr_t reqs = moveset.getCancelReqAddr(moveset.iterateCancel(addr, i));
        moveset.disableStoryRelatedReqs(reqs);
      }
      return markMovesetEdited(movesetAddr);
    }

    if (bossCode == BossCodes::FinalHeihachi)
    {
      // Health regenration prop
      // {
      //   addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      //   addr = moveset.findMoveExtraprop(addr, ExtraMoveProperties::_0x8555);
      //   for (int i = 0; i < 2; i++)
      //   {
      //     uintptr_t reqAddr = moveset.getCancelReqAddr(addr);
      //     moveset.editRequirement(moveset.iterateRequirements(reqAddr, 0), 0, 0); // 1st req
      //     moveset.editRequirement(moveset.iterateRequirements(reqAddr, 1), 0, 0); // 2nd req
      //     addr = moveset.iterateExtraprops(addr, 1);
      //   }
      // }

      // Activating Heat on idle stance
      {
        auto iterReq = [&moveset](uintptr_t addr, int n) {
          return moveset.iterateRequirements(addr, n);
        };
        // He_sKAM00_shadow
        uintptr_t addr = moveset.getMoveAddress(0xb36a0b80, moveset.getAliasMoveId(0x8000));
        uintptr_t reqList = moveset.getMoveNthCancel1stReqAddr(addr);
        // Preparing req-list
        addr = moveset.editRequirement(reqList, Requirements::HEAT_AVAILABLE, 1, 0);
        addr = moveset.editRequirement(addr, Requirements::PLAYER_IN_HEAT, 0, 0);
        addr = moveset.editRequirement(addr, Requirements::HEAT_ACTIVE_RELATED2, 0, 0);
        addr = moveset.editRequirement(addr, 0x8382, 1, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEI_WARRIOR, 1, 0);
        addr = moveset.editRequirement(addr, 0x83f4, 1, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::MULTILEVEL_INSTALLS, 3, 0);
        addr = moveset.editRequirement(addr, 0x8139, 1, 1);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::ADD_HEAT_VALUE, 900, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEAT_RELATED, 300, 0);
        addr = moveset.editRequirement(addr, ExtraMoveProperties::HEAT_AURA_VFX, 0, 0);
        addr = moveset.editRequirement(addr, Requirements::EOL, 0, 0);
        // Assigning this reqList to idle stance
        uintptr_t cancel = moveset.getMoveNthCancel(moveset.getMoveAddrByIdx(0x8001));
        moveset.editMoveCancel(cancel, 0, reqList, moveset.findCancelExtradata(10240), 1, 32767, 1, 0x8001, 257);
      }

      // Activating WI in intro against Lidia
      {
        addr = moveset.getMoveAddress(0xE323DEDC, defaultAliasIdx);
        addr = moveset.findMoveExtraprop(addr, ExtraMoveProperties::HEI_WARRIOR);
        uintptr_t reqAddr = moveset.getExtrapropValue(addr, "requirements");
        uintptr_t reqHeader = moveset.getMovesetHeader("requirements");
        // Iterating all extraprops to disable all props that have a requirement addr
        while (true)
        {
          if (moveset.getExtrapropValue(addr, "requirements") == reqAddr)
          {
            moveset.editExtrapropValue(addr, "requirements", reqHeader);
          }
          int prop = moveset.getExtrapropValue(addr, "prop");
          int frame = moveset.getExtrapropValue(addr, "frame");
          if (prop == 0 && frame == 0)
            break;
          addr = moveset.iterateExtraprops(addr, 1);
        }
      }

      // Enable most of the moves by modifying 2,2
      addr = moveset.getMoveAddress(0xF69E2BEF, 1550);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      int new22 = moveset.getCancelMoveId(addr);
      addr = moveset.getMoveAddrByIdx(new22);
      // 2,2,2
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 5));
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 6));
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 7));
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 8));
      // 1,1 > 1+3 throw
      addr = moveset.getMoveAddress(0x10E04C8A, 2000);
      moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 0));
      // Parry cancels from idle stance
      addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::DLC_STORY1_FLAGS, 3);
      // 4-cancels for parries
      if (!shouldDisableAutoParries())
      {
        for (int i = 0; i < 4; i++)
        {
          moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(moveset.iterateCancel(addr, i)));
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
    uintptr_t addr = moveset.getMoveAddress(0xc8c48167, moveset.getAliasMoveId(0x8030));
    addr = moveset.getMoveNthCancel(addr);
    addr = moveset.findCancelByCondition(addr, Requirements::ARCADE_BATTLE);
    moveset.disableRequirement(moveset.getCancelReqAddr(addr), Requirements::ARCADE_BATTLE);
    addr = moveset.iterateCancel(addr, 1); // Next cancel
    moveset.disableRequirement(moveset.getCancelReqAddr(addr), Requirements::ARCADE_BATTLE);
    // Move this in the beginning if I figure out how to get the correct intros to play
    adjustIntroOutroReq(moveset, bossCode, 2900); // I know targetReq is first seen after index 2900
    return markMovesetEdited(movesetAddr);
  }

  bool loadStoryDevilJin(uintptr_t movesetAddr, int bossCode)
  {
    if (!isValidDevilJinBoss(bossCode))
      return false;
    TkMoveset moveset(this->game, movesetAddr, this->decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    uintptr_t addr = 0;

    adjustIntroOutroReq(moveset, bossCode, 2000); // I know targetReq is first seen after index 2000

    // Adjusting winposes
    {
      int enderId = moveset.getMoveId(0xAB7FA036, defaultAliasIdx); // Grabbed ID of the match-ender
      // Grabbing ID of the first intro from alias 0x8000
      addr = moveset.getMoveAddrByIdx(defaultAliasIdx);
      addr = moveset.getMoveNthCancel(addr, 1); // 2nd Cancel
      int start = moveset.getCancelMoveId(addr);

      uintptr_t cancel = 0;
      addr = moveset.getMoveAddress(0xD9CDC1C0, start);
      for (int i = 0; i < 3; i++)
      {
        cancel = moveset.getMoveNthCancel(addr, 0);
        moveset.editCancelMoveId(cancel, enderId);
        addr += Sizes::Moveset::Move;
      }

      // Rage Art init (0xa02e070b)
      addr = moveset.getMoveAddress(0xa02e070b, defaultAliasIdx - 20);
      addr = moveset.getMoveExtrapropAddr(addr);
      // `getCancelReqAddr` can also be used to grab extraprop's req addr
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      // Rage Art throw (0xfe2cd621)
      addr = moveset.getMoveAddress(0xfe2cd621, defaultAliasIdx - 15);
      // 1st extraprop
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
      // 5th extraprop
      addr = moveset.iterateExtraprops(addr, 4);
      moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
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

    // cmp r9d, 6 / je check_cam
    emit({0x41, 0x83, 0xF9, 0x06});
    size_t jeCheckCamJin = emitRel32Hole({0x0F, 0x84});
    // cmp r9d, 35 / je check_cam
    emit({0x41, 0x83, 0xF9, 0x23});
    size_t jeCheckCamHei = emitRel32Hole({0x0F, 0x84});
    // jmp skip
    size_t jmpSkipChar = emitRel32Hole({0xE9});

    size_t checkCam = code.size();
    patchRel32(jeCheckCamJin, checkCam);
    patchRel32(jeCheckCamHei, checkCam);

    // cmp r8d, 0x24 / jb check_p2
    emit({0x41, 0x83, 0xF8, 0x24});
    size_t jbCheckP2 = emitRel32Hole({0x0F, 0x82});
    // cmp r8d, 0x29 / ja check_p2
    emit({0x41, 0x83, 0xF8, 0x29});
    size_t jaCheckP2 = emitRel32Hole({0x0F, 0x87});

    // --- P1 ---
    // cmp r9d, 6 / jne p1_hei
    emit({0x41, 0x83, 0xF9, 0x06});
    size_t jneP1Hei = emitRel32Hole({0x0F, 0x85});

    auto emitP1JinCompare = [&](uint32_t bossCode, size_t &jeRemapHole)
    {
      emit({0x81, 0x38});
      emitU32(bossCode);
      jeRemapHole = emitRel32Hole({0x0F, 0x84});
    };

    size_t jeRemapP1Jin[5];
    emitP1JinCompare(BossCodes::NerfedJin, jeRemapP1Jin[0]);
    emitP1JinCompare(BossCodes::MishimaJin, jeRemapP1Jin[1]);
    emitP1JinCompare(BossCodes::KazamaJin, jeRemapP1Jin[2]);
    emitP1JinCompare(BossCodes::FinalJin, jeRemapP1Jin[3]);
    emitP1JinCompare(BossCodes::ChainedJin, jeRemapP1Jin[4]);
    size_t jmpSkipP1Jin = emitRel32Hole({0xE9});

    size_t p1Hei = code.size();
    patchRel32(jneP1Hei, p1Hei);
    emit({0x81, 0x38});
    emitU32(BossCodes::AmnesiaHeihachi);
    size_t jeRemapP1Hei0 = emitRel32Hole({0x0F, 0x84});
    emit({0x81, 0x38});
    emitU32(BossCodes::ShadowHeihachi);
    size_t jeRemapP1Hei1 = emitRel32Hole({0x0F, 0x84});
    size_t jmpSkipP1Hei = emitRel32Hole({0xE9});

    size_t checkP2 = code.size();
    patchRel32(jbCheckP2, checkP2);
    patchRel32(jaCheckP2, checkP2);

    // cmp r8d, 0x2A / jb skip
    emit({0x41, 0x83, 0xF8, 0x2A});
    size_t jbSkipP2Lo = emitRel32Hole({0x0F, 0x82});
    // cmp r8d, 0x2F / ja skip
    emit({0x41, 0x83, 0xF8, 0x2F});
    size_t jaSkipP2Hi = emitRel32Hole({0x0F, 0x87});
    // cmp r9d, 6 / jne p2_hei
    emit({0x41, 0x83, 0xF9, 0x06});
    size_t jneP2Hei = emitRel32Hole({0x0F, 0x85});

    auto emitP2JinCompare = [&](uint32_t bossCode, size_t &jeRemapHole)
    {
      emit({0x81, 0x78, 0x04});
      emitU32(bossCode);
      jeRemapHole = emitRel32Hole({0x0F, 0x84});
    };

    size_t jeRemapP2Jin[5];
    emitP2JinCompare(BossCodes::NerfedJin, jeRemapP2Jin[0]);
    emitP2JinCompare(BossCodes::MishimaJin, jeRemapP2Jin[1]);
    emitP2JinCompare(BossCodes::KazamaJin, jeRemapP2Jin[2]);
    emitP2JinCompare(BossCodes::FinalJin, jeRemapP2Jin[3]);
    emitP2JinCompare(BossCodes::ChainedJin, jeRemapP2Jin[4]);
    size_t jmpSkipP2Jin = emitRel32Hole({0xE9});

    size_t p2Hei = code.size();
    patchRel32(jneP2Hei, p2Hei);
    emit({0x81, 0x78, 0x04});
    emitU32(BossCodes::AmnesiaHeihachi);
    size_t jeRemapP2Hei0 = emitRel32Hole({0x0F, 0x84});
    emit({0x81, 0x78, 0x04});
    emitU32(BossCodes::ShadowHeihachi);
    size_t jeRemapP2Hei1 = emitRel32Hole({0x0F, 0x84});
    size_t jmpSkipP2Hei = emitRel32Hole({0xE9});

    size_t remap = code.size();
    emit({0x41, 0x81, 0xC0});
    emitU32(CAMERA_ID_STORY_DELTA);

    size_t skip = code.size();
    patchRel32(jeSkipEligible, skip);
    patchRel32(jmpSkipChar, skip);
    patchRel32(jmpSkipP1Jin, skip);
    patchRel32(jmpSkipP1Hei, skip);
    patchRel32(jbSkipP2Lo, skip);
    patchRel32(jaSkipP2Hi, skip);
    patchRel32(jmpSkipP2Jin, skip);
    patchRel32(jmpSkipP2Hei, skip);
    for (size_t hole : jeRemapP1Jin)
      patchRel32(hole, remap);
    patchRel32(jeRemapP1Hei0, remap);
    patchRel32(jeRemapP1Hei1, remap);
    for (size_t hole : jeRemapP2Jin)
      patchRel32(hole, remap);
    patchRel32(jeRemapP2Hei0, remap);
    patchRel32(jeRemapP2Hei1, remap);

    // pop rax
    emit({0x58});
    // lea rbp, [rsp-0x1B0]
    emit({0x48, 0x8D, 0xAC, 0x24, 0x50, 0xFE, 0xFF, 0xFF});
    // jmp returnAddr — rel32 patched after cave is allocated
    emit({0xE9});
    emitU32(0);
    (void)returnAddr;

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

    // Cave must be within ±2GB of the hook — E9 rel32 cannot reach a far VirtualAllocEx.
    cameraCodeCave = reinterpret_cast<uint8_t *>(
        game.allocateNearInTarget(hookAddr, shellcode.size(), PAGE_EXECUTE_READWRITE));
    if (!cameraCodeCave)
    {
      AppendLog("Story camera hook: failed to allocate near code cave");
      game.freeInTarget(cameraRemoteState);
      cameraRemoteState = nullptr;
      return false;
    }

    uintptr_t caveAddr = reinterpret_cast<uintptr_t>(cameraCodeCave);
    if (!GameClass::isRel32Reachable(hookAddr + 5, caveAddr) ||
        !GameClass::isRel32Reachable(caveAddr + shellcode.size(), returnAddr))
    {
      AppendLog("Story camera hook: allocated cave not reachable via rel32");
      game.freeInTarget(cameraCodeCave);
      game.freeInTarget(cameraRemoteState);
      cameraCodeCave = nullptr;
      cameraRemoteState = nullptr;
      return false;
    }

    // Final instruction is E9 rel32; patch rel32 = returnAddr - (end of jmp insn)
    size_t jmpRelOffset = shellcode.size() - 4;
    int32_t rel32 = static_cast<int32_t>(
        static_cast<int64_t>(returnAddr) - static_cast<int64_t>(caveAddr + shellcode.size()));
    shellcode[jmpRelOffset + 0] = static_cast<uint8_t>(rel32 & 0xFF);
    shellcode[jmpRelOffset + 1] = static_cast<uint8_t>((rel32 >> 8) & 0xFF);
    shellcode[jmpRelOffset + 2] = static_cast<uint8_t>((rel32 >> 16) & 0xFF);
    shellcode[jmpRelOffset + 3] = static_cast<uint8_t>((rel32 >> 24) & 0xFF);

    if (!game.writeBytes(caveAddr, shellcode.data(), shellcode.size()))
    {
      AppendLog("Story camera hook: failed to write code cave");
      game.freeInTarget(cameraCodeCave);
      game.freeInTarget(cameraRemoteState);
      cameraCodeCave = nullptr;
      cameraRemoteState = nullptr;
      return false;
    }

    // Patch: E9 rel32 + NOP NOP NOP
    uint8_t patch[CAMERA_HOOK_PATCH_SIZE] = {0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90};
    int32_t hookRel = static_cast<int32_t>(
        static_cast<int64_t>(caveAddr) - static_cast<int64_t>(hookAddr + 5));
    patch[1] = static_cast<uint8_t>(hookRel & 0xFF);
    patch[2] = static_cast<uint8_t>((hookRel >> 8) & 0xFF);
    patch[3] = static_cast<uint8_t>((hookRel >> 16) & 0xFF);
    patch[4] = static_cast<uint8_t>((hookRel >> 24) & 0xFF);

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

    if (ADJUST_RA_CAMERA)
    {
      installStoryCameraHook(); // best-effort; failure must not block boss loading
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
      Sleep(100);

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
         bossCode == BossCodes::ShadowHeihachi;
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
