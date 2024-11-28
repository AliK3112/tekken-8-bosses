#include "game.h"
#include "tekken.h"
#include "utils.h"
#include <unistd.h>
#include <algorithm>

using namespace Tekken;

GameClass game;
std::map<std::string, uintptr_t> addresses;
uintptr_t MOVESET_OFFSET = 0;
uintptr_t PLAYER_STRUCT_BASE = 0;
int END_REQ = 1100;

void sleep(int ms) { usleep(ms * 1000); }
void mainFunc();
bool disableCamera(uintptr_t movesetAddr);
void storeAddresses();
uintptr_t getMoveAddress(uintptr_t moveset, int moveNameKey, int start);
uintptr_t getMoveNthCancel(uintptr_t move, int n);
uintptr_t getMoveNthCancel1stReqAddr(uintptr_t move, int n);
void disableCameraReqs(uintptr_t requirements);
bool movesetExists(uintptr_t moveset);
bool isMovesetEdited(uintptr_t moveset);
bool markMovesetEdited(uintptr_t moveset);

struct EncryptedValue
{
  uintptr_t value;
  uintptr_t key;
};

int main()
{
  printf("Waiting for Tekken 8 to run...\n");
  while (true)
  {
    if (game.Attach(L"Polaris-Win64-Shipping.exe")) {
      break;
    }
    sleep(1000);
  }
  addresses = readKeyValuePairs("addresses.txt");
  storeAddresses();
  mainFunc();
  return 0;
}

void mainFunc()
{
  bool isWritten1 = false;
  bool isWritten2 = false;
  bool flag = false;

  uintptr_t player1 = 0, moveset1 = 0;
  uintptr_t player2 = 0, moveset2 = 0;

  while (true)
  {
    sleep(1000);

    player1 = game.getAddress({(DWORD)PLAYER_STRUCT_BASE, 0x30});
    if (player1 == 0)
    {
      continue;
    }

    player2 = game.getAddress({(DWORD)PLAYER_STRUCT_BASE, 0x38});
    moveset1 = game.readUInt64(player1 + MOVESET_OFFSET);
    moveset2 = game.readUInt64(player2 + MOVESET_OFFSET);
    if (moveset1 == 0 || moveset2 == 0)
    {
      flag = false;
      isWritten1 = false;
      isWritten2 = false;
      continue;
    }

    if (!movesetExists(moveset1) || !movesetExists(moveset2))
    {
      isWritten1 = false;
      isWritten2 = false;
      continue;
    }

    isWritten1 = isMovesetEdited(moveset1);
    isWritten2 = isMovesetEdited(moveset2);

    if (!isWritten1)
    {
      isWritten1 = disableCamera(moveset1);
      if (!flag && isWritten1)
        printf("Tornado Camera Disabled\n");
      flag = true;
    }

    if (!isWritten2)
    {
      isWritten2 = disableCamera(moveset2);
    }
  }
}

void storeAddresses()
{
  PLAYER_STRUCT_BASE = getValueByKey(addresses, "player_struct_base");
  MOVESET_OFFSET = getValueByKey(addresses, "moveset_offset");
}

bool disableCamera(uintptr_t moveset)
{
  // anim-key 0x2a1eb12b (air bounds)
  uintptr_t addr = getMoveAddress(moveset, 0x2a1eb12b);
  disableCameraReqs(getMoveNthCancel1stReqAddr(addr, 0));
  disableCameraReqs(getMoveNthCancel1stReqAddr(addr, 1));
  markMovesetEdited(moveset);
  return true;
}

uintptr_t getMoveAddress(uintptr_t moveset, int moveNameKey, int start = 0)
{
  uintptr_t movesHead = game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
  int movesCount = game.readInt32(moveset + Offsets::Moveset::MovesCount);
  start = start >= movesCount ? 0 : start;
  for (int i = start; i < movesCount; i++)
  {
    uintptr_t addr = (movesHead + i * Sizes::Moveset::Move);
    int value = game.readInt32(addr + Offsets::Move::AnimAddr1);
    if (value == moveNameKey)
      return addr;
  }
  return 0;
}

uintptr_t getMoveNthCancel(uintptr_t move, int n)
{
  return game.readUInt64(move + Offsets::Move::CancelList) + Sizes::Moveset::Cancel * n;
}

uintptr_t getMoveNthCancel1stReqAddr(uintptr_t move, int n)
{
  uintptr_t cancel = getMoveNthCancel(move, n);
  return game.readUInt64(cancel + Offsets::Cancel::RequirementsList);
}

void disableCameraReqs(uintptr_t requirements)
{
  for (uintptr_t addr = requirements; true; addr += Sizes::Requirement)
  {
    int req = game.readUInt32(addr);
    if (req == END_REQ)
      break;
    if (req == 0x8695 || req == 0x8697)
    {
      game.write<uintptr_t>(addr, 0);
    }
  }
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
