#include "game.h"
#include "tekken.h"
#include "structs.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

using namespace Tekken;

const int ALIASES = 60;

std::vector<int> STORY_REQS = {
    Requirements::STORY_BATTLE,
    Requirements::STORY_BATTLE_NUM,
    Requirements::STORY_FLAGS,
    Requirements::DLC_STORY1_BATTLE,
    Requirements::DLC_STORY1_BATTLE_NUM,
    Requirements::DLC_STORY1_FLAGS,
};

uintptr_t getItemAddress(uintptr_t start, u_int index, size_t size)
{
  return start ? start + size * index : 0;
}

int getItemIndex(uintptr_t start, uintptr_t addr, size_t size)
{
  if (!start || !addr || addr < start)
    return -1;
  return static_cast<int>((addr - start) / size);
}

class TkMoveset
{
private:
  uintptr_t moveset;
  uintptr_t decryptFuncAddr;
  GameClass &game;
  Motbin motbin{};
  std::unordered_map<int, int> nameKeyToIndex;

  // Helper methods
  uintptr_t getAddressFromIndex(std::string column, uintptr_t value, size_t size)
  {
    uintptr_t start = getMovesetHeader(column);
    uintptr_t count = getMovesetCount(column);
    uintptr_t addr = getItemAddress(start, value, size);
    if (addr >= start && addr < getItemAddress(start, count - 1, size))
      return addr;
    else
      return 0;
  }

  void cacheMotbin()
  {
    motbin = {};
    if (!moveset)
      return;
    motbin = game.read<Motbin>(moveset);
  }

  void cacheMoves()
  {
    nameKeyToIndex.clear();
    if (!moveset || !motbin.moves_ptr || motbin.moves_count <= 0)
      return;

    const size_t tableSize = static_cast<size_t>(motbin.moves_count) * Sizes::Moveset::Move;
    std::vector<uint8_t> blob(tableSize);
    if (!game.readBytes(motbin.moves_ptr, blob.data(), tableSize))
      return;

    nameKeyToIndex.reserve(static_cast<size_t>(motbin.moves_count));
    for (int i = 0; i < motbin.moves_count; i++)
    {
      const uint8_t *movePtr = blob.data() + static_cast<size_t>(i) * Sizes::Moveset::Move;
      int rawIdx = (i % 8) - 4;
      int nameKey = 0;
      if (rawIdx > -1)
      {
        memcpy(&nameKey, movePtr + 0x10 + rawIdx * 4, sizeof(int));
      }
      else
      {
        EncryptedValue encrypted;
        memcpy(&encrypted, movePtr, sizeof(EncryptedValue));
        nameKey = static_cast<int>(validateAndTransform64BitValue(&encrypted));
      }
      nameKeyToIndex[nameKey] = i;
    }
  }

  int findMoveIndexLive(int moveNameKey)
  {
    uintptr_t head = motbin.moves_ptr;
    int count = motbin.moves_count;
    if (!head || count <= 0)
      return -1;
    for (int i = 0; i < count; i++)
    {
      int rawIdx = (i % 8) - 4;
      uintptr_t addr = head + i * Sizes::Moveset::Move;
      if (rawIdx > -1)
      {
        int value = game.readInt32(addr + 0x10 + rawIdx * 4);
        if (value == moveNameKey)
          return i;
      }
      else
      {
        EncryptedValue encrypted = game.read<EncryptedValue>(addr);
        uintptr_t decryptedValue = validateAndTransform64BitValue(&encrypted);
        if ((int)decryptedValue == moveNameKey)
          return i;
      }
    }
    return -1;
  }

  int findMoveIndex(int moveNameKey)
  {
    auto it = nameKeyToIndex.find(moveNameKey);
    if (it != nameKeyToIndex.end())
      return it->second;
    // Don't find it live, if it doesn't exist, it doesn't exist
    // if (nameKeyToIndex.empty())
    //   return findMoveIndexLive(moveNameKey);
    return -1;
  }

public:
  // Constructor
  TkMoveset(GameClass &game, uintptr_t moveset, uintptr_t decryptFuncAddr)
      : game(game), moveset(moveset), decryptFuncAddr(decryptFuncAddr)
  {
    cacheMotbin();
    cacheMoves();
  }

  ~TkMoveset()
  {
    this->moveset = 0;
    this->decryptFuncAddr = 0;
    this->motbin = {};
    this->nameKeyToIndex.clear();
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
    cacheMotbin();
    cacheMoves();
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

  // Disable a single requirement given a requirement list address
  void disableRequirement(uintptr_t requirements, int targetReq)
  {
    if (!requirements)
      return;
    uintptr_t addr = requirements;
    uintptr_t start = getMovesetHeader("requirements");
    uintptr_t count = getMovesetCount("requirements");
    uintptr_t end = getItemAddress(start, count - 1, sizeof(TK_Requirement));
    while (addr >= start && addr < end)
    {
      int req = getRequirementValue(addr, "req");
      if (req == targetReq)
      {
        editRequirement(addr, 0, 0);
        break;
      }
      if (req == Requirements::EOL)
        break;
      addr = iterateRequirements(addr, 1);
    }
  }

  bool replaceRequirements(int targetReq, int targetParam = -1, int overrideReq = 0, int overrideParam = 0)
  {
    uintptr_t requirements = getMovesetHeader("requirements");
    size_t requirementsCount = getMovesetCount("requirements");
    for (size_t i = 0; i < requirementsCount; i++)
    {
      uintptr_t addr = requirements + i * sizeof(TK_Requirement);
      int req = game.ReadSignedInt(addr);
      int param = game.ReadSignedInt(addr + 4);
      if (req == targetReq && (param == targetParam || targetParam == -1))
      {
        game.write<int>(addr, overrideReq);
        game.write<int>(addr + 4, overrideParam);
      }
    }
    return true;
  }

  int getMoveId(int moveNameKey, int start = 0)
  {
    int idx = findMoveIndex(moveNameKey);
    if (idx < 0)
    {
      std::ostringstream oss;
      oss << "Failed to find ID for move 0x" << std::hex << moveNameKey;
      throw std::runtime_error(oss.str());
    }
    return idx;
  }

  uintptr_t getMoveAddress(int moveNameKey, int start = 0)
  {
    int idx = findMoveIndex(moveNameKey);
    if (idx < 0)
    {
      std::ostringstream oss;
      oss << "Failed to find the desired address: moveNameKey=0x" << std::hex << moveNameKey;
      throw std::runtime_error(oss.str());
    }
    uintptr_t head = motbin.moves_ptr;
    return head + idx * Sizes::Moveset::Move;
  }

  void disableStoryRelatedReqs(uintptr_t requirements, int givenReq = Requirements::CHARA_CONTROLLER)
  {
    if (!requirements)
      return;
    uintptr_t start = getMovesetHeader("requirements");
    uintptr_t count = getMovesetCount("requirements");
    uintptr_t end = getItemAddress(start, count - 1, sizeof(TK_Requirement));
    for (uintptr_t addr = requirements; addr >= start && addr < end; addr += sizeof(TK_Requirement))
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
    return cancel ? game.readUInt64(cancel + Offsets::Cancel::RequirementsList) : 0;
  }

  uintptr_t getMoveNthCancel(uintptr_t move, int n = 0)
  {
    return move ? game.readUInt64(move + Offsets::Move::CancelList) + Sizes::Moveset::Cancel * n : 0;
  }

  uintptr_t getMoveNthCancel1stReqAddr(uintptr_t move, int n = 0)
  {
    return getCancelValue(getMoveNthCancel(move, n), "requirements");
  }

  // Returns the address of cancel extradata given index
  uintptr_t getCancelExtradataAddr(int index)
  {
    uintptr_t start = getMovesetHeader("cancel_extra_datas");
    size_t count = getMovesetCount("cancel_extra_datas");
    return getItemAddress(start, index, Sizes::Moveset::CancelExtradata);
  }

  uintptr_t findCancelExtradata(int target)
  {
    uintptr_t start = getMovesetHeader("cancel_extra_datas");
    size_t count = getMovesetCount("cancel_extra_datas");
    for (size_t i = 0; i < count; i++)
    {
      uintptr_t addr = start + i * Sizes::Moveset::CancelExtradata;
      if (game.readInt32(addr) == target)
        return addr;
    }
    return 0;
  }

  uintptr_t getMoveAddrByIdx(int idx)
  {
    if (idx < 0)
      return 0;
    uintptr_t start = getMovesetHeader("moves");
    idx = idx >= 0x8000 ? getAliasMoveId(idx) : idx;
    if (!start)
      return 0;
    size_t count = getMovesetCount("moves");
    uintptr_t addr = getItemAddress(start, idx, Sizes::Moveset::Move);
    uintptr_t end = getItemAddress(start, count - 1, Sizes::Moveset::Move);
    return addr >= start && addr < end ? addr : 0; // Not letting overflow happen
  }

  uintptr_t getMoveIdxByAddress(uintptr_t addr)
  {
    uintptr_t start = getMovesetHeader("moves");
    uintptr_t end = getMovesetHeader("voiceclips");
    if (addr >= start && addr < end) {
      return (addr - start) / Sizes::Moveset::Move;
    }
    return 0;
  }

  uintptr_t getMoveExtrapropAddr(uintptr_t move)
  {
    return move ? game.readUInt64(move + Offsets::Move::ExtraPropList) : 0;
  }

  TK_ExtraProp getExtraProp(uintptr_t addr)
  {
    if (!addr)
      return {};
    return game.read<TK_ExtraProp>(addr);
  }

  uintptr_t getExtrapropValue(uintptr_t addr, std::string column)
  {
    if (column == "frame")
      return game.readInt32(addr + Offsets::ExtraProp::Type);
    else if (column == "requirements")
      return game.readUInt64(addr + Offsets::ExtraProp::RequirementAddr);
    else if (column == "requirement_idx")
    {
      uintptr_t header = getMovesetHeader("requirements");
      uintptr_t value = game.readUInt64(addr + Offsets::ExtraProp::RequirementAddr);
      return getItemIndex(header, value, sizeof(TK_Requirement));
    }
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

  void editExtrapropValue(uintptr_t addr, std::string column, uintptr_t value)
  {
    if (addr == 0)
      return;
    if (column == "frame")
      game.write<int>(addr + Offsets::ExtraProp::Type, value);
    else if (column == "requirements")
      game.write<uintptr_t>(addr + Offsets::ExtraProp::RequirementAddr, value);
    else if (column == "requirement_idx")
    {
      uintptr_t tAddr = getAddressFromIndex("requirements", value, sizeof(TK_Requirement));
      if (!tAddr) return;
      game.write<uintptr_t>(addr + Offsets::ExtraProp::RequirementAddr, tAddr);
    }
    else if (column == "prop")
      game.write<int>(addr + Offsets::ExtraProp::Prop, value);
    else if (column == "value")
      game.write<int>(addr + Offsets::ExtraProp::Value, value);
    else if (column == "value2")
      game.write<int>(addr + Offsets::ExtraProp::Value2, value);
    else if (column == "value3")
      game.write<int>(addr + Offsets::ExtraProp::Value3, value);
    else if (column == "value4")
      game.write<int>(addr + Offsets::ExtraProp::Value4, value);
    else if (column == "value5")
      game.write<int>(addr + Offsets::ExtraProp::Value5, value);
  }

  // Moves `n` Extraprops forward given a prop's address
  uintptr_t iterateExtraprops(uintptr_t addr, int n)
  {
    return addr ? addr + n * sizeof(TK_ExtraProp) : 0;
  }

  void editExtraprop(uintptr_t propAddr, int propId, int paramValue = -1)
  {
    if (propAddr == 0)
      return;
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
    if (!cancel)
      return;
    game.write<uintptr_t>(cancel + Offsets::Cancel::RequirementsList, value);
  }

  int getAliasMoveId(int idx)
  {
    idx = idx & 0x0FFF;
    if (idx < 0 || idx >= ALIASES)
      return -1;
    return motbin.original_aliases[idx];
  }

  bool cancelHasCondition(uintptr_t cancel, int targetReq, int targetParam = -1)
  {
    if (!cancel)
      return false;
    uintptr_t requirements = getCancelValue(cancel, "requirements");
    return reqListHas(requirements, targetReq, targetParam);
  }

  uintptr_t findRequirement(uintptr_t requirement, int targetReq, int targetParam = -1)
  {
    if (!requirement)
      return 0;
    uintptr_t start = getMovesetHeader("requirements");
    uintptr_t count = getMovesetCount("requirements");
    uintptr_t end = getItemAddress(start, count - 1, sizeof(TK_Requirement));
    while (requirement >= start && requirement < end)
    {
      TK_Requirement req = getRequirement(requirement);
      if (req.req == targetReq && (req.param[0] == targetParam || targetParam == -1))
        return requirement;
      if (req.req == Requirements::EOL)
        break;
      requirement = iterateRequirements(requirement, 1);
    }
    return 0;
  }

  uintptr_t findMoveCancelByCondition(uintptr_t move, int targetReq, int targetParam = -1, int start = 0)
  {
    if (!move)
      return 0;
    start = start < 0 ? 0 : start;
    uintptr_t cancel = getMoveNthCancel(move, start);
    return findCancelByCondition(cancel, targetReq, targetParam);
  }

  uintptr_t findCancelByCondition(uintptr_t cancel, int targetReq, int targetParam = -1)
  {
    if (!cancel)
      return 0;
    uintptr_t start = getMovesetHeader("cancels");
    uintptr_t count = getMovesetCount("cancels");
    uintptr_t end = getItemAddress(start, count - 1, Sizes::Moveset::Cancel);
    while (cancel >= start && cancel < end)
    {
      if (cancelHasCondition(cancel, targetReq, targetParam))
        return cancel;
      if (getCancelValue(cancel, "command") == 0x8000)
        return 0;
      cancel += Sizes::Moveset::Cancel;
    }
    return 0;
  }

  uintptr_t findCancel(uintptr_t cancel, std::string column, uintptr_t value, bool isGroupCancel = false)
  {
    if (!cancel)
      return 0;
    uintptr_t start = getMovesetHeader(isGroupCancel ? "group_cancels" : "cancels");
    uintptr_t count = getMovesetCount(isGroupCancel ? "group_cancels" : "cancels");
    uintptr_t end = getItemAddress(start, count - 1, Sizes::Moveset::Cancel);
    uintptr_t endValue = isGroupCancel ? Cancels::GROUP_CANCEL_END : Cancels::CANCEL_END;
    while (cancel >= start && cancel < end)
    {
      if (getCancelValue(cancel, "command") == endValue)
        return 0;
      if (getCancelValue(cancel, column) == value)
        return cancel;
      cancel += Sizes::Moveset::Cancel;
    }
    return 0;
  }

  uintptr_t findExtraProp(uintptr_t addr, int targetProp, int targetFrame = -1, int targetParam = -1)
  {
    if (!addr)
      return 0;
    uintptr_t start = getMovesetHeader("extra_move_properties");
    uintptr_t count = getMovesetCount("extra_move_properties");
    uintptr_t end = getItemAddress(start, count - 1, sizeof(TK_ExtraProp));
    while (addr >= start && addr < end)
    {
      TK_ExtraProp prop = getExtraProp(addr);
      if (!prop.property && !prop.frame && !prop.params[0])
        break;
      if ((targetFrame == prop.frame || targetFrame == -1) && targetProp == prop.property && (targetParam == prop.params[0] || targetParam == -1))
      {
        return addr;
      }
      addr += sizeof(TK_ExtraProp);
    }
    return 0;
  }

  // Returns address of the next requirement
  uintptr_t editRequirement(uintptr_t addr, int req, int param = -1, int param2 = -1, int param3 = -1, int param4 = -1)
  {
    if (addr == 0)
      return 0;
    if (req != -1)
      game.write<int>(addr, req);
    if (param != -1)
      game.write<int>(addr + 4, param);
    if (param2 != -1)
      game.write<int>(addr + 8, param2);
    if (param3 != -1)
      game.write<int>(addr + 12, param3);
    if (param4 != -1)
      game.write<int>(addr + 16, param4);
    return iterateRequirements(addr, 1);
  }

  bool reqListHas(uintptr_t addr, int tReq, int tParam = -1)
  {
    if (!addr)
      return 0;
    uintptr_t start = getMovesetHeader("requirements");
    uintptr_t count = getMovesetCount("requirements");
    uintptr_t end = getItemAddress(start, count - 1, sizeof(TK_Requirement));
    while (addr >= start && addr < end)
    {
      TK_Requirement requirement = getRequirement(addr);
      if (requirement.req == tReq && (requirement.param[0] == tParam || tParam == -1))
        return true;
      if (requirement.req == Requirements::EOL)
        break;
      addr = iterateRequirements(addr, 1);
    }
    return false;
  }

  TK_Requirement getRequirement(uintptr_t addr)
  {
    if (!addr)
      return {};
    return game.read<TK_Requirement>(addr);
  }

  int getRequirementValue(uintptr_t addr, std::string column)
  {
    if (!addr)
      return 0;
    if (column == "req")
      return game.readInt32(addr);
    else if (column == "param")
      return game.readInt32(addr + 4);
    else if (column == "param2")
      return game.readInt32(addr + 8);
    else if (column == "param3")
      return game.readInt32(addr + 12);
    else if (column == "param4")
      return game.readInt32(addr + 16);
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
      game.write<uintptr_t>(targetCancelAddr + Offsets::Cancel::Command, command);
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
    if (!cancel)
      return;
    game.write<uintptr_t>(cancel + Offsets::Cancel::Command, value);
  }

  void editCancelCommand(uintptr_t cancel, int value)
  {
    if (!cancel)
      return;
    game.write<int>(cancel + Offsets::Cancel::Command, value);
  }

  void editCancelExtradata(uintptr_t cancel, uintptr_t extradataAddr)
  {
    if (!cancel || !extradataAddr)
      return;
    game.write<int>(cancel + Offsets::Cancel::CancelExtradata, extradataAddr);
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
    return cancel ? game.readInt16(cancel + Offsets::Cancel::Move) : -1;
  }

  uintptr_t getCancelValue(uintptr_t addr, std::string column)
  {
    if (!addr) return 0;
    if (column == "command")
      return game.readUInt64(addr + Offsets::Cancel::Command);
    else if (column == "requirements")
      return game.readUInt64(addr + Offsets::Cancel::RequirementsList);
    else if (column == "requirement_idx")
    {
      uintptr_t header = getMovesetHeader("requirements");
      uintptr_t value = game.readUInt64(addr + Offsets::Cancel::RequirementsList);
      return getItemIndex(header, value, sizeof(TK_Requirement));
    }
    else if (column == "extradata")
      return game.readUInt64(addr + Offsets::Cancel::CancelExtradata);
    else if (column == "extradata_idx")
    {
      uintptr_t header = getMovesetHeader("cancel_extra_datas");
      uintptr_t value = game.readUInt64(addr + Offsets::Cancel::CancelExtradata);
      return getItemIndex(header, value, Sizes::Moveset::CancelExtradata);
    }
    else if (column == "start")
      return game.readUInt32(addr + Offsets::Cancel::WindowStart);
    else if (column == "end")
      return game.readUInt32(addr + Offsets::Cancel::WindowEnd);
    else if (column == "transition")
      return game.readUInt32(addr + Offsets::Cancel::TransitionFrame);
    else if (column == "move")
      return game.readUInt16(addr + Offsets::Cancel::Move);
    else if (column == "option")
      return game.readUInt16(addr + Offsets::Cancel::Option);

    return 0;
  }

  void editCancelValue(uintptr_t addr, std::string column, uintptr_t value)
  {
    if (!addr) return;
    if (column == "command")
      game.write<uintptr_t>(addr + Offsets::Cancel::Command, value);
    else if (column == "requirements")
      game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, value);
    else if (column == "requirement_idx")
    {
      uintptr_t tAddr = getAddressFromIndex("requirements", value, sizeof(TK_Requirement));
      if (!tAddr) return;
      game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, tAddr);
    }
    else if (column == "extradata")
      game.write<uintptr_t>(addr + Offsets::Cancel::CancelExtradata, value);
    else if (column == "extradata_idx")
    {
      uintptr_t tAddr = getAddressFromIndex("cancel_extra_datas", value, Sizes::Moveset::CancelExtradata);
      if (!tAddr) return;
      game.write<uintptr_t>(addr + Offsets::Cancel::CancelExtradata, tAddr);
    }
    else if (column == "start")
      game.write<int>(addr + Offsets::Cancel::WindowStart, value);
    else if (column == "end")
      game.write<int>(addr + Offsets::Cancel::WindowEnd, value);
    else if (column == "transition")
      game.write<int>(addr + Offsets::Cancel::TransitionFrame, value);
    else if (column == "move")
      game.write<uint16_t>(addr + Offsets::Cancel::Move, value);
    else if (column == "option")
      game.write<uint16_t>(addr + Offsets::Cancel::Option, value);
  }

  // Moves `n` cancels forward given a cancel's address
  uintptr_t iterateCancel(uintptr_t cancel, int n)
  {
    return cancel ? cancel + (n * Sizes::Moveset::Cancel) : 0;
  }

  // Moves `n` requirements forward given a requirement's address
  uintptr_t iterateRequirements(uintptr_t requirement, int n)
  {
    return requirement ? (requirement + (n * sizeof(TK_Requirement))) : 0;
  }

  // Hit Conditions
  uintptr_t getMoveHitCondition(uintptr_t move)
  {
    return move ? game.readUInt64(move + Offsets::Move::HitConditionList) : 0;
  }

  uintptr_t getMoveNthHitCondition(uintptr_t move, int n = 0)
  {
    return move ? getMoveHitCondition(move) + Sizes::Moveset::HitCondition * n : 0;
  }

  bool isLastHitCondition(uintptr_t addr)
  {
    if (!addr) return true;
    return getRequirementValue(game.readUInt64(addr), "req") == Requirements::EOL;
  }

  // Moves `n` hit-conditions forward given a hit-conditions's address
  uintptr_t iterateHitConditions(uintptr_t hitCondition, int n)
  {
    return hitCondition ? (hitCondition + (n * Sizes::Moveset::HitCondition)) : 0;
  }

  void editHitConditionValue(uintptr_t addr, std::string column, uintptr_t value)
  {
    if (!addr)
      return;
    if (column == "requirement")
      game.write<uintptr_t>(addr + Offsets::HitCondition::RequirementAddrHC,
                            value);
    else if (column == "requirement_idx")
    {
      uintptr_t tAddr = getAddressFromIndex("requirements", value, sizeof(TK_Requirement));
      if (!tAddr) return;
      game.write<uintptr_t>(addr + Offsets::HitCondition::RequirementAddrHC, tAddr);
    }
    else if (column == "damage")
      game.write<int>(addr + Offsets::HitCondition::Damage, value);
    else if (column == "reaction")
      game.write<uintptr_t>(addr + Offsets::HitCondition::ReactionListAddr, value);
    else if (column == "reaction_idx")
    {
      uintptr_t tAddr = getAddressFromIndex("reactions", value, Sizes::Moveset::ReactionList);
      if (!tAddr) return;
      game.write<uintptr_t>(addr + Offsets::HitCondition::ReactionListAddr, tAddr);
    }
  }

  void replaceCancelMoveIndexes(std::vector<std::pair<int, int>> moves, bool groupCancels = false)
  {
    auto func = [&](std::string section)
    {
      const uintptr_t start = getMovesetHeader(section);
      const uintptr_t count = getMovesetCount(section);

      for (uintptr_t i = 0; i < count; ++i)
      {
        const uintptr_t addr = start + i * Sizes::Cancel;
        const int cMoveId = getCancelValue(addr, "move");

        for (const auto &[targetMoveId, replacementMoveId] : moves)
        {
          if (cMoveId == targetMoveId)
          {
            editCancelValue(addr, "move", replacementMoveId);
          }
        }
      }
    };

    func("cancels");
    if (groupCancels) func("group_cancels");
  }

  uintptr_t getMovesetHeader(std::string column)
  {
    if (column == "reactions")
      return motbin.reactions_ptr;
    else if (column == "requirements")
      return motbin.requirements_ptr;
    else if (column == "hit_conditions")
      return motbin.hit_conditions_ptr;
    else if (column == "projectiles")
      return motbin.projectiles_ptr;
    else if (column == "pushbacks")
      return motbin.pushbacks_ptr;
    else if (column == "pushback_extra_data")
      return motbin.pushback_extradata_ptr;
    else if (column == "cancels")
      return motbin.cancels_ptr;
    else if (column == "group_cancels")
      return motbin.group_cancels_ptr;
    else if (column == "cancel_extra_datas")
      return motbin.cancel_extradata_ptr;
    else if (column == "extra_move_properties")
      return motbin.extra_move_properties_ptr;
    else if (column == "move_start_props")
      return motbin.move_start_props_ptr;
    else if (column == "move_end_props")
      return motbin.move_end_props_ptr;
    else if (column == "moves")
      return motbin.moves_ptr;
    else if (column == "voiceclips")
      return motbin.voiceclips_ptr;
    else if (column == "input_sequences")
      return motbin.input_sequences_ptr;
    else if (column == "input_extra_data")
      return motbin.inputs_ptr;
    else if (column == "parry_list")
      return motbin.parryable_list_ptr;
    else if (column == "throw_extras")
      return motbin.throw_extras_ptr;
    else if (column == "throws")
      return motbin.throws_ptr;
    else if (column == "dialogues")
      return motbin.dialogues_ptr;

    return 0;
  }

  uintptr_t getMovesetCount(std::string column)
  {
    if (column == "reactions")
      return motbin.reactions_count;
    else if (column == "requirements")
      return motbin.requirements_count;
    else if (column == "hit_conditions")
      return motbin.hit_conditions_count;
    else if (column == "projectiles")
      return motbin.projectiles_count;
    else if (column == "pushbacks")
      return motbin.pushbacks_count;
    else if (column == "pushback_extra_data")
      return motbin.pushback_extradata_count;
    else if (column == "cancels")
      return motbin.cancels_count;
    else if (column == "group_cancels")
      return motbin.group_cancels_count;
    else if (column == "cancel_extra_datas")
      return motbin.cancel_extradata_count;
    else if (column == "extra_move_properties")
      return motbin.extra_move_properties_count;
    else if (column == "move_start_props")
      return motbin.move_start_props_count;
    else if (column == "move_end_props")
      return motbin.move_end_props_count;
    else if (column == "moves")
      return motbin.moves_count;
    else if (column == "voiceclips")
      return motbin.voiceclips_count;
    else if (column == "input_sequences")
      return motbin.input_sequences_count;
    else if (column == "input_extra_data")
      return motbin.inputs_count;
    else if (column == "parry_list")
      return motbin.parryable_list_count;
    else if (column == "throw_extras")
      return motbin.throw_extras_count;
    else if (column == "throws")
      return motbin.throws_count;
    else if (column == "dialogues")
      return motbin.dialogues_count;

    return 0;
  }
};
