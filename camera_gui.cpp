#include <windows.h>
#include <string>
#include <cstdarg>
#include <process.h>
#include "moveset.h"

const char CLASS_NAME[] = "CleanRadioWindow";

GameClass game;
HWND hwndLogBox;
HWND hwndCheckTornado, hwndCheckHeat;

uintptr_t PLAYER_STRUCT_BASE = 0;
uintptr_t MOVESET_OFFSET = 0;
uintptr_t DECRYPT_FUNC_ADDR = 0;
bool ATTACHED = false;
bool READY = false;
bool CHECK_TORNADO = false;
bool CHECK_HEAT_BURST = false;

void AppendLog(const char *format, ...);
void InitializeUI(HWND hwnd);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
unsigned int __stdcall AttachToGameThread(void *param);
void AttachToGame();
void scanAddresses();
void mainLoop();
bool movesetExists(uintptr_t moveset);
bool isMovesetEdited(uintptr_t moveset);
bool markMovesetEdited(uintptr_t moveset);
bool removeCameras(int side);
void disableCameraReqs(uintptr_t requirements);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
  WNDCLASSA wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(CLASS_NAME, "Moveset Camera Tweaker",
                            WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
                            CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
                            NULL, NULL, hInst, NULL);
  if (!hwnd)
    return 0;

  InitializeUI(hwnd);
  ShowWindow(hwnd, nCmdShow);

  HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, AttachToGameThread, NULL, 0, NULL);

  MSG msg = {};
  while (GetMessageA(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  return 0;
}

void InitializeUI(HWND hwnd)
{
  int padding = 20;
  int spacing = 10;
  int y = padding;

  hwndCheckTornado = CreateWindowA("BUTTON", "Disable Tornado Camera", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   padding, y, 300, 20, hwnd, (HMENU)101, NULL, NULL);
  y += 30;

  hwndCheckHeat = CreateWindowA("BUTTON", "Disable Heat Burst Camera", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                padding, y, 300, 20, hwnd, (HMENU)102, NULL, NULL);
  y += 30;

  hwndLogBox = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                             padding, y, 340, 80, hwnd, NULL, NULL, NULL);
}

void AppendLog(const char *format, ...)
{
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, sizeof(buffer), format, args);
  va_end(args);

  int len = GetWindowTextLengthA(hwndLogBox);
  SendMessageA(hwndLogBox, EM_SETSEL, len, len);
  SendMessageA(hwndLogBox, EM_REPLACESEL, 0, (LPARAM)(std::string(buffer) + "\r\n").c_str());
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
  case WM_COMMAND:
    switch (LOWORD(wp))
    {
    case 101:
    {
      BOOL checked = SendMessageA(hwndCheckTornado, BM_GETCHECK, 0, 0) == BST_CHECKED;
      CHECK_TORNADO = checked;
      break;
    }
    case 102:
    {
      BOOL checked = SendMessageA(hwndCheckHeat, BM_GETCHECK, 0, 0) == BST_CHECKED;
      CHECK_HEAT_BURST = checked;
      break;
    }
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProcA(hwnd, msg, wp, lp);
  }
  return 0;
}

unsigned int __stdcall AttachToGameThread(void *param)
{
  AttachToGame();
  return 0;
}

void AttachToGame()
{
  AppendLog("Waiting for game to start...");
  while (true)
  {
    if (game.Attach(L"Polaris-Win64-Shipping.exe"))
    {
      AppendLog("Successfully attached to game!");
      ATTACHED = true;
      break;
    }
    Sleep(1000);
  }
  scanAddresses();

  if (READY)
  {
    mainLoop();
  }
}

void scanAddresses()
{
  uintptr_t addr = 0;
  uintptr_t base = game.getBaseAddress();
  uintptr_t start = base;
  addr = game.FastAoBScan(Tekken::PLAYER_STRUCT_SIG_BYTES, base + 0x5A00000);
  if (addr != 0)
  {
    addr = addr + 7 + game.readUInt32(addr + 3) - base;
    PLAYER_STRUCT_BASE = addr;
    AppendLog("Player Struct Base Address: 0x%llx", addr);
  }
  else
  {
    PLAYER_STRUCT_BASE = 0;
    AppendLog("Player Struct Base Address not found!");
  }

  addr = game.FastAoBScan(Tekken::MOVSET_OFFSET_SIG_BYTES, base + 0x1800000);
  if (addr != 0)
  {
    addr = game.readUInt32(addr + 3);
    MOVESET_OFFSET = addr;
    AppendLog("Moveset Offset: 0x%llx", addr);
  }
  else
  {
    MOVESET_OFFSET = 0;
    AppendLog("Moveset Offset not found!");
  }

  addr = game.FastAoBScan(Tekken::ENC_SIG_BYTES, base + 0x1700000);
  if (addr != 0)
  {
    DECRYPT_FUNC_ADDR = addr;
    AppendLog("Decryption Function Address: 0x%llx", addr - base);
  }
  else
  {
    AppendLog("Decryption Function Address not found!");
  }

  READY = true;
}

uintptr_t getPlayerAddress(int player)
{
  return game.getAddress({(DWORD)PLAYER_STRUCT_BASE, (DWORD)(0x30 + player * 8)});
}

uintptr_t getMovesetAddress(uintptr_t playerAddr)
{
  return playerAddr ? game.readUInt64(playerAddr + MOVESET_OFFSET) : 0;
}

void mainLoop()
{
  uintptr_t player = 0, moveset = 0;
  while (true)
  {
    Sleep(100);

    player = getPlayerAddress(0);
    if (!player)
      continue;

    moveset = getMovesetAddress(player);
    if (!movesetExists(moveset))
      continue;

    if (isMovesetEdited(moveset))
      continue;

    if (removeCameras(0) && removeCameras(1))
    {
      AppendLog("Moveset Modified!");
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

bool removeCameras(int side)
{
  uintptr_t player = getPlayerAddress(side);
  if (!player)
    return false;

  uintptr_t movesetAddr = getMovesetAddress(player);
  if (!movesetAddr)
    return false;

  TkMoveset moveset(game, movesetAddr, DECRYPT_FUNC_ADDR);

  if (CHECK_TORNADO)
  {
    uintptr_t addr = moveset.getMoveAddressByAnimKey(0x2a1eb12b);
    disableCameraReqs(moveset.getMoveNthCancel1stReqAddr(addr, 0));
    disableCameraReqs(moveset.getMoveNthCancel1stReqAddr(addr, 1));
  }

  if (CHECK_HEAT_BURST)
  {
    int idleStanceIdx = moveset.getAliasMoveId(0x8001);
    uintptr_t addr = moveset.getMoveAddrByIdx(idleStanceIdx);
    // Finding the 1st group cancel inside idle stance cancel list
    addr = moveset.getMoveNthCancel(addr);
    addr = moveset.findCancel(addr, "command", Cancels::GROUP_CANCEL_START);
    int groupCancelStart = moveset.getCancelValue(addr, "move");
    int groupCancelLength = moveset.getCancelValue(addr, "transition");
    uintptr_t groupCancelHeader = moveset.getMovesetHeader("group_cancels");
    addr = moveset.getItemAddress(groupCancelHeader, groupCancelStart, Sizes::Moveset::Cancel);
    while (true)
    {
      addr = moveset.findCancel(addr, "command", 0x4000000600000020, true);
      if (addr)
      {
        // Finding the cancel that has the conditions for Heat Burst cancel
        if (
            moveset.cancelHasCondition(addr, Requirements::HEAT_AVAILABLE, 1) &&
            moveset.cancelHasCondition(addr, Requirements::NOT_BACKTURNED) &&
            moveset.cancelHasCondition(addr, Requirements::_452))
        {
          uintptr_t move = moveset.getMoveAddrByIdx(moveset.getCancelValue(addr, "move"));
          // Finding and disabling the heat camera props
          addr = moveset.getMoveExtrapropAddr(move);
          addr = moveset.findExtraProp(addr, ExtraMoveProperties::HEAT_CAMERA);
          moveset.editExtraprop(addr, 0);
          // Finding and disabling the 2 opponent visibility props
          addr = moveset.findExtraProp(addr, ExtraMoveProperties::OPP_VISIBILTY);
          moveset.editExtraprop(addr, 0);
          addr = moveset.iterateExtraprops(addr, 1);
          moveset.editExtraprop(addr, 0);

          break;
        }
        else
        {
          addr = moveset.iterateCancel(addr, 1); // going to next cancel
        }
      }
      else
        break;
    }
  }

  return (CHECK_TORNADO || CHECK_HEAT_BURST) ? markMovesetEdited(movesetAddr) : false;
}

void disableCameraReqs(uintptr_t requirements)
{
  for (uintptr_t addr = requirements; true; addr += Sizes::Requirement)
  {
    int req = game.readUInt32(addr);
    if (req == Requirements::EOL)
      break;
    if (req == ExtraMoveProperties::CAMERA_TRANSITION || req == ExtraMoveProperties::CAMERA_ORBIT)
    {
      game.write<uintptr_t>(addr, 0);
    }
  }
}
