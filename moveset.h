#include "game.h"
#include "tekken.h"
#include <algorithm>

using namespace Tekken;

std::vector<int> STORY_REQS = {
    Requirements::STORY_BATTLE,
    Requirements::STORY_BATTLE_NUM,
    Requirements::STORY_FLAGS,
    Requirements::DLC_STORY1_BATTLE,
    Requirements::DLC_STORY1_BATTLE_NUM,
    Requirements::DLC_STORY1_FLAGS,
};

struct EncryptedValue
{
  uintptr_t value;
  uintptr_t key;
};

class TkMoveset
{
private:
  uintptr_t moveset;
  uintptr_t decryptFuncAddr;
  GameClass &game;

public:
  // Constructor
  TkMoveset(GameClass &game, uintptr_t moveset, uintptr_t decryptFuncAddr)
      : game(game), moveset(moveset), decryptFuncAddr(decryptFuncAddr) {}

  ~TkMoveset()
  {
    this->moveset = 0;
    this->decryptFuncAddr = 0;
  }

  // Getter for moveset
  uintptr_t getMoveset() const
  {
    return moveset;
  }

  // Setter for moveset
  void setMoveset(uintptr_t newMoveset)
  {
    moveset = newMoveset;
  }

  // Getter for game
  GameClass getGame() const
  {
    return game;
  }

  // Setter for game
  void setGame(const GameClass &newGame)
  {
    game = newGame;
  }

  // Utility methods
  bool disableRequirements(int targetReq, int targetParam)
  {
    uintptr_t requirements = game.ReadUnsignedLong(moveset + Offsets::Moveset::RequirementsHeader);
    int requirementsCount = game.ReadSignedInt(moveset + Offsets::Moveset::RequirementsCount);
    for (int i = 0; i < requirementsCount; i++)
    {
      uintptr_t addr = requirements + i * Sizes::Requirement;
      int req = game.ReadSignedInt(addr);
      int param = game.ReadSignedInt(addr + 4);
      if (req == targetReq && param == targetParam)
      {
        game.write<uint64_t>(addr, 0);
      }
    }
    return true;
  }

  int getMoveId(int moveNameKey, int start = 0)
  {
    uintptr_t movesHead = game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
    int movesCount = game.readUInt64(moveset + Offsets::Moveset::MovesCount);
    start = start >= movesCount ? 0 : start;
    uintptr_t addr = 0;
    int rawIdx = -1;
    for (int i = start; i < movesCount; i++)
    {
      rawIdx = (i % 8) - 4;
      addr = movesHead + i * Sizes::Moveset::Move;
      if (rawIdx > -1)
      {
        int value = game.readInt32(addr + 0x10 + rawIdx * 4);
        if (value == moveNameKey)
          return i;
      }
      else
      {
        EncryptedValue *paramAddr = reinterpret_cast<EncryptedValue *>(addr);
        uintptr_t decryptedValue = game.callFunction<uintptr_t, EncryptedValue>(decryptFuncAddr, paramAddr);
        if ((int)decryptedValue == moveNameKey)
          return i;
      }
    }
    std::ostringstream oss;
    oss << "Failed to find the desired address: moveNameKey=0x" << std::hex << moveNameKey;
    throw std::runtime_error(oss.str());
    return -1;
  }

  uintptr_t getMoveAddress(int moveNameKey, int start = 0)
  {
    uintptr_t movesHead = game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
    int movesCount = game.readInt32(moveset + Offsets::Moveset::MovesCount);
    start = start >= movesCount ? 0 : start;
    int rawIdx = -1;
    for (int i = start; i < movesCount; i++)
    {
      rawIdx = (i % 8) - 4;
      uintptr_t addr = movesHead + i * Sizes::Moveset::Move;
      if (rawIdx > -1)
      {
        int value = game.readInt32(addr + 0x10 + rawIdx * 4);
        if (value == moveNameKey)
          return addr;
      }
      else
      {
        EncryptedValue *paramAddr = reinterpret_cast<EncryptedValue *>(addr);
        uintptr_t decryptedValue = game.callFunction<uintptr_t, EncryptedValue>(decryptFuncAddr, paramAddr);
        if ((int)decryptedValue == moveNameKey)
          return addr;
      }
    }
    std::ostringstream oss;
    oss << "Failed to find the desired address: moveNameKey=0x" << std::hex << moveNameKey;
    throw std::runtime_error(oss.str());
  }

  void disableStoryRelatedReqs(uintptr_t requirements, int givenReq = Requirements::CHARA_CONTROLLER)
  {
    for (uintptr_t addr = requirements; true; addr += Sizes::Requirement)
    {
      int req = game.readUInt32(addr);
      if (req == Requirements::EOL)
        break;
      if ((std::find(STORY_REQS.begin(), STORY_REQS.end(), req) != STORY_REQS.end()) || req == givenReq)
      {
        game.write<uintptr_t>(addr, 0);
      }
    }
  }

  uintptr_t getCancelReqAddr(uintptr_t cancel)
  {
    return game.readUInt64(cancel + Offsets::Cancel::RequirementsList);
  }

  uintptr_t getMoveNthCancel(uintptr_t move, int n)
  {
    return game.readUInt64(move + Offsets::Move::CancelList) + Sizes::Moveset::Cancel * n;
  }

  uintptr_t getMoveNthCancel1stReqAddr(uintptr_t move, int n)
  {
    return getCancelReqAddr(getMoveNthCancel(move, n));
  }
};
