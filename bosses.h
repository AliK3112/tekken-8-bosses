// This class will be responsible for loading boss characters
#include "moveset.h"
#include "utils.h"

bool DEV_MODE = false;

using namespace Tekken;

std::string FINAL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_naked.CS_ant_1p_naked";
std::string CHAINED_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_chain.CS_ant_1p_chain";
std::string FINAL_KAZ_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_grl_1p_v2_white.CS_grl_1p_v2_white";
std::string DEVIL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_swl_ant_1p.CS_swl_ant_1p";
std::string HEIHACHI_MONK_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_whitetiger_nohat_nomask.CS_bee_whitetiger_nohat_nomask";
std::string HEIHACHI_SHADOW_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_1p_p_shadow.CS_bee_1p_p_shadow";

bool isValidJinBoss(int bossCode);
bool isValidKazuyaBoss(int bossCode);
bool isValidHeihachiBoss(int bossCode);
bool isCorrectHeihachiFlag(int storyFlag, int param);

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
  uintptr_t decryptFuncAddr = 0;
  uintptr_t hudIconAddr = 0;
  uintptr_t hudNameAddr = 0;
  // CONFIGURATIONS
  bool handleIcons = false;
  HWND hwndLogBox;

  // METHODS
  // Append message to log box
  void AppendLog(const std::string &msg)
  {
    if (msg.empty())
      return; // Prevent appending empty messages

    // Get current text length
    int length = GetWindowTextLengthA(hwndLogBox);

    // Ensure no excessive newlines
    std::string trimmedMsg = msg;
    while (!trimmedMsg.empty() && (trimmedMsg.back() == '\n' || trimmedMsg.back() == '\r'))
    {
      trimmedMsg.pop_back();
    }

    // Append text properly
    SendMessageA(hwndLogBox, EM_SETSEL, length, length);
    SendMessageA(hwndLogBox, EM_REPLACESEL, 0, (LPARAM)(trimmedMsg + "\r\n").c_str());
  }

  // Append message to log box (overloaded for formatted strings)
  void AppendLog(const char *format, ...)
  {
    char buffer[255]; // Adjust size as needed
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    // vprintf(format, args);
    va_end(args);

    AppendLog(std::string(buffer));
  }

  void setKazuyaPermaDevil(uintptr_t playerAddr, int value)
  {
    if (!playerAddr || !permaDevilOffset)
      return;
    game.write<int>(playerAddr + permaDevilOffset, value);
  }

  // Checks if it's eligible to load the boss character
  bool isEligible(uintptr_t matchStructAddr)
  {
    int value = game.readInt32(matchStructAddr);
    return value == 1 || value == 2 || value == 5 || value == 6 || value == 12;
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

    addr = game.FastAoBScan(Tekken::ENC_SIG_BYTES, base + 0x1700000);
    if (addr != 0)
    {
      decryptFuncAddr = addr;
    }
    else
    {
      throw std::runtime_error("Decryption Function Address not found!");
    }

    addr = game.FastAoBScan(Tekken::HUD_ICON_SIG_BYTES, start);
    hudIconAddr = addr + 13;

    addr = game.FastAoBScan(Tekken::HUD_NAME_SIG_BYTES, addr + 0x10, addr + 0x1000);
    hudNameAddr = addr + 13;

    // Setting the global flag
    handleIcons = hudIconAddr && hudNameAddr;

    addr = game.FastAoBScan(Tekken::MOVSET_OFFSET_SIG_BYTES, decryptFuncAddr + 0x1000);
    if (addr != 0)
    {
      movesetOffset = game.readUInt32(addr + 3);
    }
    else
    {
      throw std::runtime_error("\"Moveset\" Offset not found!");
    }

    addr = game.FastAoBScan(Tekken::DEVIL_FLAG_SIG_BYTES, base + 0x2C00000);
    if (addr != 0)
    {
      permaDevilOffset = game.readUInt32(addr + 3);
    }
    else
    {
      throw std::runtime_error("\"Permanent Devil Mode\" offset not found!");
    }

    if (DEV_MODE)
    {
      printf("playerStructOffset: 0x%llX\n", playerStructOffset);
      printf("matchStructOffset: 0x%llX\n", matchStructOffset);
      printf("decryptFuncAddr: 0x%llX\n", decryptFuncAddr);
      printf("hudIconAddr: 0x%llX\n", hudIconAddr);
      printf("hudNameAddr: 0x%llX\n", hudNameAddr);
      printf("movesetOffset: 0x%llX\n", movesetOffset);
      printf("permaDevilOffset: 0x%llX\n", permaDevilOffset);
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
    if (bossCode == BossCodes::DevilJin && charId == FighterId::DevilJin2)
    {
      icon = buildString(c, getCharCode(FighterId::Jin));
      name = getNamePath(FighterId::Jin);
    }
    else if ((bossCode == BossCodes::FinalJin || bossCode == BossCodes::MishimaJin || bossCode == BossCodes::KazamaJin) && charId == FighterId::Jin)
    {
      icon = buildString(c, "ant2");
      name = getNamePath(FighterId::Jin);
    }
    else if (bossCode == BossCodes::FinalKazuya && charId == FighterId::Kazuya)
    {
      icon = buildString(c, "grl2");
      name = getNamePath(FighterId::Kazuya);
    }
    else if (bossCode == BossCodes::DevilKazuya && charId == FighterId::Kazuya)
    {
      icon = buildString(c, "grl3");
      name = getNamePath("grl2");
    }
    else if (bossCode == BossCodes::AmnesiaHeihachi && charId == FighterId::Heihachi)
    {
      icon = buildString(c, "bee2");
      name = getNamePath(FighterId::Heihachi);
    }
    else if (bossCode == BossCodes::ShadowHeihachi && charId == FighterId::Heihachi)
    {
      icon = buildString(c, "bee3");
      name = getNamePath("bee3");
    }
    if (!icon.empty())
      game.writeString(matchStruct + 0x2C0 + side * 0x100, icon);
    if (!name.empty())
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
    default:
      return;
    }
    if (charId != -1 && isEligible(matchStructAddr))
    {
      setCharId(matchStructAddr, side, charId);
      if (charId == BossCodes::DevilJin)
      {
        loadCostume(matchStructAddr, side, 51, DEVIL_JIN_COSTUME_PATH); // Just a safety precaution
      }
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
    std::string costumePath;
    switch (bossCode)
    {
    case BossCodes::RegularJin:
    case BossCodes::NerfedJin:
    case BossCodes::DevilKazuya:
    case BossCodes::FinalHeihachi:
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
      costumePath = DEVIL_JIN_COSTUME_PATH;
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
    loadCostume(matchStructAddr, side, 51, costumePath);
  }

  void adjustIntroOutroReq(TkMoveset& moveset, int bossCode, int start = 0)
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
      // Cancels & Props both have requirements at offset 0x8
      if (moveset.cancelHasCondition(addr, Requirements::DLC_STORY1_BATTLE_NUM, 2050))
      {
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(addr));
        break;
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
    moveset.disableRequirements(Requirements::STORY_FLAGS, _777param);

    // Adjusting Rage Art
    uintptr_t rageArt = moveset.getMoveAddress(0x9BAE061E, 2100);
    if (rageArt && bossCode != BossCodes::RegularJin)
    {
      uintptr_t cancel = moveset.getMoveNthCancel(rageArt, 0);
      moveset.editCancelMoveId(cancel, (short)moveset.getMoveId(0x1ADAB0CB, 2000));
    }

    // Disabling glowing eyes for new season 2 ZEN > CD cancels
    if (bossCode != BossCodes::RegularJin)
    {
      // FC df4 ~ f ~ df
      uintptr_t addr = moveset.getMoveAddress(0xDA8608B7, 1750);
      addr = moveset.getMoveExtrapropAddr(addr);
      // Disabling first 3 props
      moveset.editExtraprop(moveset.iterateExtraprops(addr, 0), 0);
      moveset.editExtraprop(moveset.iterateExtraprops(addr, 1), 0);
      moveset.editExtraprop(moveset.iterateExtraprops(addr, 2), 0);

      // ZEN 1+2 ~ df
      addr = moveset.getMoveAddress(0x459C84C1, 1750);
      addr = moveset.getMoveExtrapropAddr(addr);
      addr = moveset.iterateExtraprops(addr, 2);
      // Disabling the next 3 props
      moveset.editExtraprop(moveset.iterateExtraprops(addr, 0), 0);
      moveset.editExtraprop(moveset.iterateExtraprops(addr, 1), 0);
      moveset.editExtraprop(moveset.iterateExtraprops(addr, 2), 0);

      // Replace the new f,f+1+2 with f,f+2
      int moveId = moveset.getMoveId(0xE383D012, 2200); // f,f+2
      if (moveId != -1)
      {
        // f,f+1+2
        try {
          addr = moveset.getMoveAddress(0xEB242623, 1750); // f,f+1+2
        } catch (...) {
          addr = 0; // If somebody is using the mod on pre-S2, this shouldn't crash
        }
        addr = moveset.getMoveNthCancel(addr, 0);
        uintptr_t extradata = moveset.findCancelExtradata(389);
        uintptr_t reqHeader = moveset.getMovesetHeader("requirements");

        // Modifying 1st cancel
        moveset.editMoveCancel(
            addr,
            0,
            reqHeader,
            extradata,
            1,
            1,
            1,
            (short)moveId,
            65);
      }
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
    break;
    case BossCodes::MishimaJin:
    case BossCodes::KazamaJin:
    {
      uintptr_t moveId = moveset.getMoveId(bossCode == 2 ? 0xA33CD19D : 0x7614EF15, 2000);
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
          moveset.editRequirement(addr, 0, 0);
        }
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
        addr = moveset.findCancelByMoveId(addr, df34_1_2);
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

      return markMovesetEdited(movesetAddr);
    }

    return false;
  }

  bool loadAzazel(uintptr_t movesetAddr, int bossCode)
  {
    if (bossCode != BossCodes::Azazel)
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);

    uintptr_t addr = moveset.getMoveAddrByIdx(defaultAliasIdx);
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
    addr = moveset.iterateExtraprops(moveset.getMoveExtrapropAddr(addr), 4); // 5th prop
    moveset.editExtraprop(addr, ExtraMoveProperties::HEI_WARRIOR, (int)(bossCode == BossCodes::FinalHeihachi));

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
      for (int i = 0; i < 4; i++)
      {
        moveset.disableStoryRelatedReqs(moveset.getCancelReqAddr(moveset.iterateCancel(addr, i)));
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
    if (bossCode != BossCodes::DevilJin)
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

public:
  GameClass game;

  TkBossLoader(int bossCode_L = BossCodes::None, int bossCode_R = BossCodes::None)
  {
    this->bossCode_L = bossCode_L;
    this->bossCode_R = bossCode_R;
    this->attached = false;
  }

  TkBossLoader(GameClass& game)
  {
    this->game = game;
    this->bossCode_L = BossCodes::None;
    this->bossCode_R = BossCodes::None;
    this->attached = false;
  }

  ~TkBossLoader()
  {
    restoreHudAddr(0);
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
  }
  void setBossCode_R(int code)
  {
    this->bossCode_R = code;
  }
  void setBossCodes(int codeL, int codeR)
  {
    this->bossCode_L = codeL;
    this->bossCode_R = codeR;
  }
  void setBossCodeForSelectedSide(int selectedSide, int bossCode)
  {
    (selectedSide != 0) ? this->bossCode_R = bossCode : this->bossCode_L = bossCode;
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
        continue;
      }

      if (handleIcons)
      {
        modifyHudAddr(matchStructAddr);
      }

      if (!isEligible(matchStructAddr))
      {
        continue;
      }

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
          AppendLog("Loaded Boss %s for Player %d", getBossName(code).c_str(), selectedSide + 1);
        }
      }
      else
      {
        if (loadBoss(this->bossCode_L, 0))
        {
          AppendLog("Loaded Boss %s for Player 1", getBossName(this->bossCode_L).c_str());
        }
        if (loadBoss(this->bossCode_R, 1))
        {
          AppendLog("Loaded Boss %s for Player 2", getBossName(this->bossCode_R).c_str());
        }
      }
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

bool isValidJinBoss(int bossCode)
{
  return bossCode == BossCodes::RegularJin ||
         bossCode == BossCodes::NerfedJin ||
         bossCode == BossCodes::MishimaJin ||
         bossCode == BossCodes::KazamaJin ||
         bossCode == BossCodes::FinalJin ||
         bossCode == BossCodes::ChainedJin;
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
