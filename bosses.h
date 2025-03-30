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

class TkBossLoader
{
private:
  // MEMBERS
  int bossCode_L = BossCodes::None;
  int bossCode_R = BossCodes::None;
  bool attached = false;
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
    va_end(args);

    AppendLog(std::string(buffer));
  }

  // Checks if it's eligible to load the boss character
  bool isEligible(uintptr_t matchStructAddr)
  {
    int value = game.readInt32(matchStructAddr);
    return value == 1 || value == 2 || value == 5 || value == 6 || value == 12;
  }

  // Side, 0 = P1, 1 = P2
  uintptr_t getPlayerAddress(int side)
  {
    return game.getAddress({(DWORD)playerStructOffset, (DWORD)(0x30 + side * 8)});
  }

  uintptr_t getMovesetAddress(int playerAddr)
  {
    return game.ReadUnsignedLong(playerAddr + movesetOffset);
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
    AppendLog("Scanning for addresses...\n");
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
    AppendLog("Addresses successfully scanned...\n");
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
    int currCharId = game.readInt32(matchStructAddr + 0x10 + side * 0x84);
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
      game.write<int>(matchStructAddr + 0x10 + side * 0x84, charId);
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

  void adjustIntroOutroReq(uintptr_t moveset, int bossCode, int start = 0)
  {
    uintptr_t reqHeader = game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
    uintptr_t reqCount = game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
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

  bool loadJin(uintptr_t movesetAddr, int bossCode)
  {
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    int _777param = bossCode == BossCodes::ChainedJin ? 1 : bossCode;
    moveset.disableRequirements(Requirements::STORY_FLAGS, _777param);

    // Adjusting Rage Art
    uintptr_t rageArt = moveset.getMoveAddress(0x9BAE061E, 2100);
    if (rageArt && bossCode != 0)
    {
      uintptr_t cancel = game.readUInt64(rageArt + Offsets::Move::CancelList);
      int regularRA = moveset.getMoveId(0x1ADAB0CB, 2000);
      if (regularRA != -1)
      {
        game.write<short>(cancel + Offsets::Cancel::Move, regularRA);
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
      uintptr_t reqHeader = moveset.getRequirementsHeader();
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
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    int idleStanceIdx = moveset.getAliasMoveId(0x8001);
    if (bossCode == BossCodes::DevilKazuya)
    {
      // Enabling permanent Devil form
      uintptr_t addr = moveset.getMoveAddrByIdx(idleStanceIdx);
      addr = moveset.getMoveExtrapropAddr(addr);
      moveset.editMoveExtraprop(addr, 0, ExtraMoveProperties::PERMA_DEVIL, 1);

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
        // Move 2 cancels forward
        addr += Sizes::Moveset::Cancel * 2;
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
      moveset.editCancelReqAddr(addr, moveset.getRequirementsHeader());

      // d/b+4
      addr = moveset.getMoveAddress(0x9364E2F5, idleStanceIdx);
      addr = moveset.findMoveCancelByCondition(addr, Requirements::STORY_BATTLE_NUM, 97);
      addr = moveset.getCancelReqAddr(addr);
      moveset.disableStoryRelatedReqs(addr);
      // Disabling standing req
      game.write<int>(addr + Sizes::Moveset::Requirement, 0);
    }
    else if (bossCode == BossCodes::FinalKazuya)
    {
      // Go through reqs and props to disable his devil form
      // requirements
      uintptr_t start = moveset.getRequirementsHeader();
      uintptr_t count = moveset.getRequirementsCount();
      for (uintptr_t i = 4530; i < count - 2000; i++)
      {
        uintptr_t addr = start + (i * Sizes::Moveset::Requirement);
        int req = game.readUInt32(addr);
        int param = game.readUInt32(addr + 4);
        if ((req == ExtraMoveProperties::DEVIL_STATE && param >= 1) || (req == ExtraMoveProperties::WING_ANIM))
        {
          moveset.editRequirement(addr, 0, 0);
        }
      }

      // extraprops
      start = moveset.getExtraMovePropsHeader();
      count = moveset.getExtraMovePropsCount();
      for (uintptr_t i = 2200; i < count; i++)
      {
        uintptr_t addr = start + (i * Sizes::Moveset::ExtraMoveProperty);
        int prop = game.readUInt32(addr + Offsets::ExtraProp::Prop);
        int param = game.readUInt32(addr + Offsets::ExtraProp::Value);
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

      uintptr_t reqHeader = moveset.getRequirementsHeader();

      // Ultra-wavedash
      addr = moveset.getMoveAddress(0x77314B09, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);

      // CD+1+2
      addr = moveset.getMoveAddress(0x0C9CE140, idleStanceIdx);
      moveset.editCancelReqAddr(moveset.getMoveNthCancel(addr, 0), reqHeader);

      // b+2,2
      addr = moveset.getMoveAddress(0x8B5BFA6C, idleStanceIdx);
      moveset.editCancelReqAddr(moveset.getMoveNthCancel(addr, 0), reqHeader);

      // NEW b+2,2. Disabling laser cancel
      addr = moveset.getMoveAddress(0x8FE28C6A, defaultAliasIdx);
      addr = moveset.getMoveNthCancel(addr, 1);
      moveset.editCancelExtradata(addr, moveset.getNthCancelExtradataAddr(60));

      // Disabling u/b+1+2 laser
      addr = moveset.getMoveAddress(0x07F32E0C, 2000);
      addr = moveset.getMoveNthCancel(addr, 0);
      moveset.editMoveCancel(
          addr,
          0,
          reqHeader,
          moveset.getNthCancelExtradataAddr(16),
          -1,
          -1,
          1,
          moveset.getMoveId(0x1376C644, idleStanceIdx),
          65);

      // d/f+3+4, 1
      addr = moveset.getMoveAddress(0x6562FA84, idleStanceIdx);
      addr = moveset.getMoveNthCancel(addr, 0);
      int moveId = moveset.getCancelMoveId(moveset.iterateCancel(addr, 1));
      moveset.editCancelMoveId(addr, (short)moveId);

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
          moveset.getNthCancelExtradataAddr(20),
          5,
          5,
          5,
          moveset.getMoveId(0x0AB42E52, defaultAliasIdx),
          65);
    }

    return markMovesetEdited(movesetAddr);
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
    adjustIntroOutroReq(movesetAddr, bossCode, 2085); // I know targetReq is first seen after index 2085

    return markMovesetEdited(movesetAddr);
  }

  bool loadTrueDevilKazuya(uintptr_t movesetAddr, int bossCode)
  {
    if (bossCode != BossCodes::TrueDevilKazuya)
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    adjustIntroOutroReq(movesetAddr, bossCode, 2900); // I know targetReq is first seen after index 2900
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    // d/f+1, 2
    uintptr_t addr = moveset.getMoveAddress(0x4339a4bd, 1673);
    moveset.disableStoryRelatedReqs(moveset.getMoveNthCancel1stReqAddr(addr, 22), 473); // 23rd cancel
    return markMovesetEdited(movesetAddr);
  }

  bool loadStoryDevilJin(uintptr_t movesetAddr, int bossCode)
  {
    if (bossCode != BossCodes::DevilJin)
      return false;
    TkMoveset moveset(this->game, movesetAddr, decryptFuncAddr);
    int defaultAliasIdx = moveset.getAliasMoveId(0x8000);
    uintptr_t addr = 0;

    adjustIntroOutroReq(movesetAddr, bossCode, 2000); // I know targetReq is first seen after index 2000

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
      // I know these are extraprops but `getCancelReqAddr` also works to grab req addresses
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
  // Utility methods
  void bossLoadMainLoop()
  {
    if (!this->attached)
      return;
    scanAddresses();
    const std::vector<DWORD> offsets = {(DWORD)matchStructOffset, 0x50, 0x8, 0x18, 0x8};
    uintptr_t matchStructAddr = game.getAddress(offsets);
    if (!matchStructAddr)
    {
      this->attached = false;
      printf("Cannot find the match structure address.\n");
      return;
    }

    // bool isWritten = false;
    // bool flag = false;
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

      loadCharacter(matchStructAddr, 0, this->bossCode_L);
      loadCharacter(matchStructAddr, 1, this->bossCode_R);

      if (handleIcons)
      {
        hudHandler(matchStructAddr);
      }

      uintptr_t playerAddr = getPlayerAddress(0);
      if (playerAddr == 0)
      {
        costumeHandler(matchStructAddr, 0, this->bossCode_L);
        costumeHandler(matchStructAddr, 1, this->bossCode_R);
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

      if (!isMovesetEdited(movesetAddr))
      {
        // isWritten = false;
      }

      // if (!isWritten)
      // {
      //   isWritten = loadBoss(this->bossCode_L, 0);
      //   if (isWritten) {
      //     AppendLog("Loaded Boss: %d", this->bossCode_L);
      //     // break;
      //   }
      // }
      if (loadBoss(this->bossCode_L, 0))
      {
        AppendLog("Loaded Boss %s for Player 1", getBossName(this->bossCode_L).c_str());
      }
      if (loadBoss(this->bossCode_R, 1))
      {
        AppendLog("Loaded Boss %s for Player 2", getBossName(this->bossCode_R).c_str());
      }

      // Sleep(100);
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

    int charId = game.readInt32(playerAddr + 0x168);
    switch (charId)
    {
    case FighterId::Jin:
      return loadJin(movesetAddr, bossCode);
    case FighterId::Kazuya:
    {
      if (bossCode == BossCodes::DevilKazuya)
      {
        game.write<int>(playerAddr + permaDevilOffset, 1);
      }
      return loadKazuya(movesetAddr, bossCode);
    }
    case FighterId::Azazel:
      return loadAzazel(movesetAddr, bossCode);
    case FighterId::Heihachi:
      // return loadHeihachi(movesetAddr, bossCode);
    case FighterId::AngelJin:
      return loadAngelJin(movesetAddr, bossCode);
    case FighterId::TrueDevilKazuya:
      return loadTrueDevilKazuya(movesetAddr, bossCode);
    case FighterId::DevilJin2:
      return loadStoryDevilJin(movesetAddr, bossCode);
    default:
      return false;
    }
  }
};
