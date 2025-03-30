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

  uintptr_t getNthCancelExtradataAddr(int n)
  {
    return game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasHeader) + Sizes::Moveset::CancelExtradata * n;
  }

  uintptr_t getRequirementsHeader()
  {
    return game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
  }

  uintptr_t getRequirementsCount()
  {
    return game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
  }

  uintptr_t getExtraMovePropsHeader()
  {
    return game.readUInt64(moveset + Offsets::Moveset::ExtraMovePropertiesHeader);
  }

  uintptr_t getExtraMovePropsCount()
  {
    return game.readUInt64(moveset + Offsets::Moveset::ExtraMovePropertiesCount);
  }

  uintptr_t getMovesHeader()
  {
    return game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
  }

  uintptr_t getMoveAddrByIdx(int idx)
  {
    uintptr_t head = game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
    return head + (idx * Sizes::Moveset::Move);
  }

  uintptr_t getMoveExtrapropAddr(uintptr_t move)
  {
    return game.readUInt64(move + Offsets::Move::ExtraPropList);
  }

  void editCancelReqAddr(uintptr_t cancel, uintptr_t value)
  {
    game.write<uintptr_t>(cancel + Offsets::Cancel::RequirementsList, value);
  }

  int getAliasMoveId(int idx)
  {
    if (idx < 0x8000 || idx > (0x8000 + 60))
      return -1;
    return game.readUInt16(moveset + 0x30 + (idx - 0x8000) * 2);
  }

  void editMoveExtraprop(uintptr_t extraPropListAddr, int idx, int prop, int propVal)
  {
    extraPropListAddr += (idx * Sizes::Moveset::ExtraMoveProperty);
    game.write<int>(extraPropListAddr + Offsets::ExtraProp::Prop, prop);
    game.write<int>(extraPropListAddr + Offsets::ExtraProp::Value, propVal);
  }

  bool cancelHasCondition(uintptr_t cancel, int targetReq, int targetParam = -1)
  {
    uintptr_t requirements = getCancelReqAddr(cancel);
    for (uintptr_t addr = requirements; true; addr += Sizes::Moveset::Requirement)
    {
      int req = game.readInt32(addr);
      int param = game.readInt32(addr + 4);
      if (req == Requirements::EOL)
        return false;
      if (req == targetReq && (targetParam == -1 || param == targetParam))
      {
        return true;
      }
    }
    return false;
  }

  uintptr_t findMoveCancelByCondition(uintptr_t move, int targetReq, int targetParam = -1)
  {
    if (!move)
      return 0;
    uintptr_t cancel = getMoveNthCancel(move, 0);
    for (; true; cancel += Sizes::Moveset::Cancel)
    {
      if (cancelHasCondition(cancel, targetReq, targetParam))
      {
        return cancel;
      }
    }
    return 0;
  }

  void editRequirement(uintptr_t addr, int req, int param = -1)
  {
    if (req != -1)
      game.write<int>(addr, 0);
    if (param != -1)
      game.write<int>(addr + 4, 0);
  }

  void editMoveCancel(
      uintptr_t targetCancelAddr,
      uintptr_t command,
      uintptr_t requirements,
      uintptr_t extradata,
      int windowStart,
      int windowEnd,
      int startingFrame,
      short moveId,
      short option)
  {
    if (targetCancelAddr == 0)
      return;
    if (command != 0)
      game.write<int>(targetCancelAddr + Offsets::Cancel::Command, command);
    if (requirements != 0)
      game.write<int>(targetCancelAddr + Offsets::Cancel::RequirementsList, requirements);
    if (extradata != 0)
      game.write<int>(targetCancelAddr + Offsets::Cancel::CancelExtradata, extradata);
    if (windowStart != -1)
      game.write<int>(targetCancelAddr + Offsets::Cancel::WindowStart, windowStart);
    if (windowEnd != -1)
      game.write<int>(targetCancelAddr + Offsets::Cancel::WindowEnd, windowEnd);
    if (startingFrame != -1)
      game.write<int>(targetCancelAddr + Offsets::Cancel::TransitionFrame, startingFrame);
    if (moveId != -1)
      game.write<short>(targetCancelAddr + Offsets::Cancel::Move, (short)moveId);
    if (option != -1)
      game.write<short>(targetCancelAddr + Offsets::Cancel::Option, (short)option);
  }

  void editCancelCommand(uintptr_t cancel, uintptr_t value)
  {
    game.write<uintptr_t>(cancel + Offsets::Cancel::Command, value);
  }

  void editCancelCommand(uintptr_t cancel, int value)
  {
    game.write<int>(cancel + Offsets::Cancel::Command, value);
  }

  void editCancelExtradata(uintptr_t cancel, uintptr_t value)
  {
    game.write<int>(cancel + Offsets::Cancel::CancelExtradata, value);
  }

  void editCancelFrames(uintptr_t cancel, int windowStart, int windowEnd, int startingFrame)
  {
    if (windowStart != -1)
      game.write<int>(cancel + Offsets::Cancel::WindowStart, windowStart);
    if (windowEnd != -1)
      game.write<int>(cancel + Offsets::Cancel::WindowEnd, windowEnd);
    if (startingFrame != -1)
      game.write<int>(cancel + Offsets::Cancel::TransitionFrame, startingFrame);
  }

  void editCancelMoveId(uintptr_t cancel, short moveId)
  {
    game.write<short>(cancel + Offsets::Cancel::Move, moveId);
  }

  void editCancelOption(uintptr_t cancel, short value)
  {
    game.write<short>(cancel + Offsets::Cancel::Option, value);
  }

  int getCancelMoveId(uintptr_t cancel)
  {
    return game.readInt16(cancel + Offsets::Cancel::Move);
  }

  // Moves `n` cancels forward given a cancel's address
  uintptr_t iterateCancel(uintptr_t cancel, int n)
  {
    return cancel + (n * Sizes::Moveset::Cancel);
  }
};
