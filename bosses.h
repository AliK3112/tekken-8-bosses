// This class will be responsible for loading boss characters
#include "moveset.h"
#include "utils.h"

bool DEV_MODE = false;

using namespace Tekken;

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
    default:
      return false;
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

    bool isWritten = false;
    bool flag = false;
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
        // TODO: modifyHudAddr
      }
      if (!isEligible(matchStructAddr))
      {
        continue;
      }

      // TODO: loadCharacter

      if (handleIcons)
      {
        // TODO: hudHandler
      }

      uintptr_t playerAddr = getPlayerAddress(0);
      if (playerAddr == 0)
      {
        // TODO: costumeHandler(matchStructAddr, bossCode);
        continue;
      }

      uintptr_t movesetAddr = getMovesetAddress(playerAddr);
      if (movesetAddr == 0)
      {
        flag = false;
        isWritten = false;
        continue;
      }

      if (handleIcons)
      {
        // TODO: restoreHudAddr(matchStructAddr);
      }

      if (!movesetExists(movesetAddr))
      {
        isWritten = false;
        continue;
      }

      if (!isMovesetEdited(movesetAddr))
      {
        isWritten = false;
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
      // return loadKazuya(movesetAddr, bossCode);
    }
    case FighterId::Azazel:
      // return loadAzazel(movesetAddr, bossCode);
    case FighterId::Heihachi:
      // return loadHeihachi(movesetAddr, bossCode);
    case FighterId::AngelJin:
      // return loadAngelJin(movesetAddr, bossCode);
    case FighterId::TrueDevilKazuya:
      // return loadTrueDevilKazuya(movesetAddr, bossCode);
    case FighterId::DevilJin2:
      // return loadStoryDevilJin(movesetAddr, bossCode);
    default:
      return false;
    }
  }
};
