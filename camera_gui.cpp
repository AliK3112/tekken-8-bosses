#include <windows.h>
#include <string>
#include <cstdarg>
#include <process.h>
#include "moveset.h"

const char CLASS_NAME[] = "CleanRadioWindow";

GameClass game;
TkMoveset moveset;
HWND hwndLogBox;
HWND hwndRadio1, hwndRadio2;

uintptr_t PLAYER_STRUCT_BASE = 0;
uintptr_t MOVESET_OFFSET = 0;
uintptr_t DECRYPT_FUNC_ADDR = 0;
bool ATTACHED = false;
bool READY = false;

void AppendLog(const char *format, ...);
void setToField(HWND hwndField, int value);
void setToField(HWND hwndField, const char *text);
void InitializeUI(HWND hwnd);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
unsigned int __stdcall AttachToGameThread(void *param);
void AttachToGame();
void scanAddresses();
void mainLoop();
bool movesetExists(uintptr_t moveset);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
  WNDCLASSA wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(CLASS_NAME, "Game Mod GUI",
                            WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
                            CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
                            NULL, NULL, hInst, NULL);
  if (!hwnd)
    return 0;

  InitializeUI(hwnd);
  ShowWindow(hwnd, nCmdShow);

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

  hwndRadio1 = CreateWindowA("BUTTON", "Disable Tornado", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                             padding, y, 300, 20, hwnd, (HMENU)101, NULL, NULL);
  y += 30;

  hwndRadio2 = CreateWindowA("BUTTON", "Placeholder Option", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                             padding, y, 300, 20, hwnd, (HMENU)102, NULL, NULL);
  y += 30;

  hwndLogBox = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                             padding, y, 340, 80, hwnd, NULL, NULL, NULL);
}

void setToField(HWND hwndField, int value)
{
  char buffer[32];
  _snprintf_s(buffer, sizeof(buffer), "%d", value);
  SetWindowTextA(hwndField, buffer);
}

void setToField(HWND hwndField, const char *text)
{
  SetWindowTextA(hwndField, text);
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
      AppendLog("Disable Tornado selected.");
      break;
    case 102:
      AppendLog("Placeholder option selected.");
      break;
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
    // mainLoop();
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
  while (true)
  {
    Sleep(100);

    if (!getPlayerAddress(0))
    {
      // setToField(hwndPlayer1, "???");
      // setToField(hwndPlayer2, "???");
      continue;
    }

    if (!movesetExists(getMovesetAddress(0)))
      continue;

    // setToField(hwndPlayer1, getCurrentMoveId(getPlayerAddress(0)));
    // setToField(hwndPlayer2, getCurrentMoveId(getPlayerAddress(1)));
  }
}

bool movesetExists(uintptr_t moveset)
{
  std::string str = game.ReadString(moveset + 8, 3);
  return str.compare("TEK") == 0;
}
