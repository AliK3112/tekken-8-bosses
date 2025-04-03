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

  uintptr_t findCancelExtradata(int target)
  {
    uintptr_t start = getMovesetHeader("cancel_extra_datas");
    uintptr_t count = getMovesetCount("cancel_extra_datas");
    for (uintptr_t i = 0; i < count; i++)
    {
      uintptr_t addr = start + i * Sizes::Moveset::CancelExtradata;
      if (game.readInt32(addr) == target)
        return addr;
    }
    return 0;
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

  uintptr_t getExtrapropValue(uintptr_t addr, std::string column)
  {
    if (column == "frame")
      return game.readInt32(addr + Offsets::ExtraProp::Type);
    else if (column == "requirements")
      return game.readUInt64(addr + Offsets::ExtraProp::RequirementAddr);
    else if (column == "prop")
      return game.readInt32(addr + Offsets::ExtraProp::Prop);
    else if (column == "value")
      return game.readInt32(addr + Offsets::ExtraProp::Value);
    else if (column == "value2")
      return game.readInt32(addr + Offsets::ExtraProp::Value2);
    else if (column == "value3")
      return game.readInt32(addr + Offsets::ExtraProp::Value3);
    else if (column == "value4")
      return game.readInt32(addr + Offsets::ExtraProp::Value4);
    else if (column == "value5")
      return game.readInt32(addr + Offsets::ExtraProp::Value5);

    return 0;
  }

  // Moves `n` Extraprops forward given a prop's address
  uintptr_t iterateExtraprops(uintptr_t addr, int n)
  {
    return addr + n * Sizes::Moveset::ExtraMoveProperty;
  }

  void editExtraprop(uintptr_t propAddr, int propId, int paramValue = -1)
  {
    if (propId != -1)
    {
      game.write<int>(propAddr + Offsets::ExtraProp::Prop, propId);
    }
    if (paramValue != -1)
    {
      game.write<int>(propAddr + Offsets::ExtraProp::Value, paramValue);
    }
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

  uintptr_t findMoveCancelByCondition(uintptr_t move, int targetReq, int targetParam = -1, int start = 0)
  {
    if (!move)
      return 0;
    start = start < 0 ? 0 : start;
    uintptr_t cancel = getMoveNthCancel(move, start);
    for (; true; cancel += Sizes::Moveset::Cancel)
    {
      if (cancelHasCondition(cancel, targetReq, targetParam))
      {
        return cancel;
      }
    }
    return 0;
  }

  uintptr_t findCancel(uintptr_t cancel, std::string column, uintptr_t value)
  {
    if (!cancel)
      return 0;
    for (; true; cancel += Sizes::Moveset::Cancel)
    {
      if (getCancelValue(cancel, "command") == 0x8000)
        return 0;
      if (getCancelValue(cancel, column) == value)
        return cancel;
    }
    return 0;
  }

  uintptr_t findCancelByMoveId(uintptr_t cancel, int targetMoveId)
  {
    if (!cancel)
      return 0;
    for (; true; cancel += Sizes::Moveset::Cancel)
    {
      if (getCancelValue(cancel, "command") == 0x8000)
        return 0;
      if (getCancelValue(cancel, "move") == targetMoveId)
        return cancel;
    }
    return 0;
  }

  uintptr_t findMoveCancelByMoveId(uintptr_t move, int targetMoveId, int start = 0)
  {
    if (!move)
      return 0;
    start = start < 0 ? 0 : start;
    return findCancelByMoveId(getMoveNthCancel(move, start), targetMoveId);
  }

  uintptr_t findMoveExtraprop(uintptr_t move, int targetProp, int targetFrame = -1, int targetParam = -1)
  {
    uintptr_t addr = getMoveExtrapropAddr(move);
    while (true)
    {
      int frame = game.readInt32(addr + Offsets::ExtraProp::Type);
      int prop = game.readInt32(addr + Offsets::ExtraProp::Prop);
      int param = game.readInt32(addr + Offsets::ExtraProp::Value);
      if (!prop && !frame && !param)
        break;
      if ((targetFrame == frame || targetFrame == -1) && targetProp == prop && (targetParam == param || targetParam == -1))
      {
        return addr;
      }
      addr += Sizes::Moveset::ExtraMoveProperty;
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

  bool reqListHas(uintptr_t addr, int tReq, int tParam = -1)
  {
    while (true)
    {
      int req = getRequirementValue(addr, "req");
      int param = getRequirementValue(addr, "param");
      if (req == tReq && (param == tParam || tParam == -1))
        return true;
      if (req == Requirements::EOL)
        break;
      addr = iterateRequirements(addr, 1);
    }
    return false;
  }

  int getRequirementValue(uintptr_t addr, std::string column)
  {
    if (column == "req")
      return game.readInt32(addr);
    else if (column == "param")
      return game.readInt32(addr + 4);
    return 0;
  }

  void editExtraprop(uintptr_t addr, int prop, int frame, int value)
  {
    if (addr == 0)
      return;
    if (prop != -1)
      game.write(addr + Offsets::ExtraProp::Prop, prop);
    if (frame != -1)
      game.write(addr + Offsets::ExtraProp::Type, frame);
    if (value != -1)
      game.write(addr + Offsets::ExtraProp::Value, value);
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
    if (cancel == 0)
      return;
    if (moveId == -1)
      return;
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

  uintptr_t getCancelValue(uintptr_t addr, std::string column)
  {
    if (column == "command")
      return game.readInt32(addr + Offsets::Cancel::Command);
    else if (column == "requirements")
      return game.readUInt64(addr + Offsets::Cancel::RequirementsList);
    else if (column == "extradata")
      return game.readUInt64(addr + Offsets::Cancel::CancelExtradata);
    else if (column == "start")
      return game.readInt32(addr + Offsets::Cancel::WindowStart);
    else if (column == "end")
      return game.readInt32(addr + Offsets::Cancel::WindowEnd);
    else if (column == "transition")
      return game.readInt32(addr + Offsets::Cancel::TransitionFrame);
    else if (column == "move")
      return game.readInt16(addr + Offsets::Cancel::Move);
    else if (column == "option")
      return game.readInt16(addr + Offsets::Cancel::Option);

    return 0;
  }

  // Moves `n` cancels forward given a cancel's address
  uintptr_t iterateCancel(uintptr_t cancel, int n)
  {
    return cancel ? cancel + (n * Sizes::Moveset::Cancel) : 0;
  }

  // Moves `n` requirements forward given a requirement's address
  uintptr_t iterateRequirements(uintptr_t requirement, int n)
  {
    return requirement + (n * Sizes::Moveset::Requirement);
  }

  uintptr_t getMovesetHeader(std::string column)
  {
    if (column == "reactions")
      return game.readUInt64(moveset + Offsets::Moveset::ReactionsHeader);
    else if (column == "requirements")
      return game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
    else if (column == "hit_conditions")
      return game.readUInt64(moveset + Offsets::Moveset::HitConditionsHeader);
    else if (column == "projectiles")
      return game.readUInt64(moveset + Offsets::Moveset::ProjectilesHeader);
    else if (column == "pushbacks")
      return game.readUInt64(moveset + Offsets::Moveset::PushbacksHeader);
    else if (column == "pushback_extra_data")
      return game.readUInt64(moveset + Offsets::Moveset::PushbackExtraDataHeader);
    else if (column == "cancels")
      return game.readUInt64(moveset + Offsets::Moveset::CancelsHeader);
    else if (column == "group_cancels")
      return game.readUInt64(moveset + Offsets::Moveset::GroupCancelsHeader);
    else if (column == "cancel_extra_datas")
      return game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasHeader);
    else if (column == "extra_move_properties")
      return game.readUInt64(moveset + Offsets::Moveset::ExtraMovePropertiesHeader);
    else if (column == "move_start_props")
      return game.readUInt64(moveset + Offsets::Moveset::MoveStartPropsHeader);
    else if (column == "move_end_props")
      return game.readUInt64(moveset + Offsets::Moveset::MoveEndPropsHeader);
    else if (column == "moves")
      return game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
    else if (column == "voiceclips")
      return game.readUInt64(moveset + Offsets::Moveset::VoiceclipsHeader);
    else if (column == "input_sequences")
      return game.readUInt64(moveset + Offsets::Moveset::InputSequencesHeader);
    else if (column == "input_extra_data")
      return game.readUInt64(moveset + Offsets::Moveset::InputExtraDataHeader);
    else if (column == "parry_list")
      return game.readUInt64(moveset + Offsets::Moveset::ParryListHeader);
    else if (column == "throw_extras")
      return game.readUInt64(moveset + Offsets::Moveset::ThrowExtrasHeader);
    else if (column == "throws")
      return game.readUInt64(moveset + Offsets::Moveset::ThrowsHeader);
    else if (column == "dialogues")
      return game.readUInt64(moveset + Offsets::Moveset::DialoguesHeader);

    return 0;
  }

  uintptr_t getMovesetCount(std::string column)
  {
    if (column == "reactions")
      return game.readUInt64(moveset + Offsets::Moveset::ReactionsCount);
    else if (column == "requirements")
      return game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
    else if (column == "hit_conditions")
      return game.readUInt64(moveset + Offsets::Moveset::HitConditionsCount);
    else if (column == "projectiles")
      return game.readUInt64(moveset + Offsets::Moveset::ProjectilesCount);
    else if (column == "pushbacks")
      return game.readUInt64(moveset + Offsets::Moveset::PushbacksCount);
    else if (column == "pushback_extra_data")
      return game.readUInt64(moveset + Offsets::Moveset::PushbackExtraDataCount);
    else if (column == "cancels")
      return game.readUInt64(moveset + Offsets::Moveset::CancelsCount);
    else if (column == "group_cancels")
      return game.readUInt64(moveset + Offsets::Moveset::GroupCancelsCount);
    else if (column == "cancel_extra_datas")
      return game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasCount);
    else if (column == "extra_move_properties")
      return game.readUInt64(moveset + Offsets::Moveset::ExtraMovePropertiesCount);
    else if (column == "move_start_props")
      return game.readUInt64(moveset + Offsets::Moveset::MoveStartPropsCount);
    else if (column == "move_end_props")
      return game.readUInt64(moveset + Offsets::Moveset::MoveEndPropsCount);
    else if (column == "moves")
      return game.readUInt64(moveset + Offsets::Moveset::MovesCount);
    else if (column == "voiceclips")
      return game.readUInt64(moveset + Offsets::Moveset::VoiceclipsCount);
    else if (column == "input_sequences")
      return game.readUInt64(moveset + Offsets::Moveset::InputSequencesCount);
    else if (column == "input_extra_data")
      return game.readUInt64(moveset + Offsets::Moveset::InputExtraDataCount);
    else if (column == "parry_list")
      return game.readUInt64(moveset + Offsets::Moveset::ParryListCount);
    else if (column == "throw_extras")
      return game.readUInt64(moveset + Offsets::Moveset::ThrowExtrasCount);
    else if (column == "throws")
      return game.readUInt64(moveset + Offsets::Moveset::ThrowsCount);
    else if (column == "dialogues")
      return game.readUInt64(moveset + Offsets::Moveset::DialoguesCount);

    return 0;
  }
};
