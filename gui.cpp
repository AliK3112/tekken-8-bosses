#include <windows.h>
#include <vector>
#include <string>
#include <cstdarg>
#include "bosses.h"

const char CLASS_NAME[] = "BossSelectorWindow";

// Global UI elements
HWND hwndLabel1, hwndLabel2, hwndCombo1, hwndCombo2, hwndButton, hwndLogBox;
TkBossLoader boss;
char buffer[255];

// Boss mapping
struct Boss
{
  int id;
  const char *name;
};

std::vector<Boss> bossList = {
    {-1, "None"},
    {0, "Jin (Boosted)"},
    {1, "Jin (Nerfed)"},
    {2, "Jin (Mishima)"},
    {3, "Jin (Kazama)"},
    {4, "Jin (Final)"},
    {11, "Jin (Chained)"},
    {117, "Jin (Angel)"},
    {121, "Jin (Devil)"},
    {97, "Kazuya (Devil)"},
    {244, "Kazuya (Final)"},
    {118, "Kazuya (True Devil)"},
    {351, "Heihachi (Monk)"},
    {352, "Heihachi (Shadow)"},
    {353, "Heihachi (Final)"},
    {32, "Azazel"},
};

// Function declarations
void InitializeUI(HWND hwnd);
void PopulateComboBox(HWND comboBox);
void AppendLog(const std::string &msg);
void AppendLog(const char *format, ...);
void AttachToGame();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
unsigned int __stdcall AttachToGameThread(void *param);

void HandleBossSelection()
{
  int idx1 = SendMessageA(hwndCombo1, CB_GETCURSEL, 0, 0);
  int idx2 = SendMessageA(hwndCombo2, CB_GETCURSEL, 0, 0);

  if (idx1 >= 0 && idx1 < bossList.size() && idx2 >= 0 && idx2 < bossList.size())
  {
    boss.setBossCodes(bossList[idx1].id, bossList[idx2].id);
  }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
  WNDCLASSA wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(CLASS_NAME, "Boss Selector",
                            WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
                            NULL, NULL, hInst, NULL);
  if (!hwnd)
    return 0;

  InitializeUI(hwnd);
  ShowWindow(hwnd, nCmdShow);

  HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, AttachToGameThread, NULL, 0, NULL);
  if (hThread)
  {
    CloseHandle(hThread);
  }

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
  const int padding = 20;
  const int comboWidth = 180;
  const int comboHeight = 200;
  const int buttonWidth = 120;
  const int buttonHeight = 30;
  const int logWidth = 440;
  const int logHeight = 100;
  const int windowWidth = 500;

  int combo1X = (windowWidth / 2) - comboWidth - 10;
  int combo2X = (windowWidth / 2) + 10;

  hwndLabel1 = CreateWindowA("STATIC", "Player 1 Boss Character",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             combo1X, padding, comboWidth, 20, hwnd, NULL, NULL, NULL);

  hwndLabel2 = CreateWindowA("STATIC", "Player 2 Boss Character",
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             combo2X, padding, comboWidth, 20, hwnd, NULL, NULL, NULL);

  hwndCombo1 = CreateWindowA("COMBOBOX", NULL,
                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             combo1X, padding + 25, comboWidth, comboHeight, hwnd, (HMENU)1, NULL, NULL);

  hwndCombo2 = CreateWindowA("COMBOBOX", NULL,
                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             combo2X, padding + 25, comboWidth, comboHeight, hwnd, (HMENU)2, NULL, NULL);

  hwndButton = CreateWindowA("BUTTON", "Apply Selection",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             (windowWidth - buttonWidth) / 2, padding + 75, buttonWidth, buttonHeight, hwnd, (HMENU)3, NULL, NULL);

  hwndLogBox = CreateWindowA("EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                             padding, 150, logWidth, logHeight, hwnd, NULL, NULL, NULL);

  PopulateComboBox(hwndCombo1);
  PopulateComboBox(hwndCombo2);

  SendMessageA(hwndCombo1, CB_SETCURSEL, 0, 0);
  SendMessageA(hwndCombo2, CB_SETCURSEL, 0, 0);
}

void PopulateComboBox(HWND comboBox)
{
  for (const auto &boss : bossList)
  {
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)boss.name);
  }
}

void AppendLog(const std::string &msg)
{
  if (msg.empty())
    return;

  int length = GetWindowTextLengthA(hwndLogBox);
  SendMessageA(hwndLogBox, EM_SETSEL, length, length);
  SendMessageA(hwndLogBox, EM_REPLACESEL, 0, (LPARAM)(msg + "\r\n").c_str());
}

void AppendLog(const char *format, ...)
{
  char buffer[255];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, sizeof(buffer), format, args);
  va_end(args);
  AppendLog(std::string(buffer));
}

void AttachToGame()
{
  AppendLog("Waiting for game to start...");
  while (true)
  {
    if (boss.attach())
    {
      AppendLog("Successfully attached to game!");
      AppendLog(buffer, "Base Address: 0x%llx", boss.game.getBaseAddress());
      break;
    }
    Sleep(1000);
  }
  boss.attachToLogBox(hwndLogBox);
  boss.bossLoadMainLoop();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
  case WM_COMMAND:
    if (LOWORD(wp) == 3)
    {
      HandleBossSelection();
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
