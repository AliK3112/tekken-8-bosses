#include <windows.h>
#include <vector>
#include <string>
#include "game.h"

// #include "game_hacking.h" // Include your game hacking logic here

const char CLASS_NAME[] = "BossSelectorWindow";

// Global UI elements
HWND hwndCombo1, hwndCombo2, hwndButton, hwndLogBox;
GameClass game;

// Boss mapping
struct Boss
{
  int id;
  const char *name;
};

std::vector<Boss> bossList = {
    {-1, "No Boss Selected"},
    {0, "Jin (Boosted)"},
    {1, "Jin (Nerfed)"},
    {2, "Jin (Mishima)"},
    {3, "Jin (Kazama)"},
    {4, "Jin (Ultimate)"},
    {11, "Jin (Chained)"},
    {32, "Azazel"},
    {97, "Kazuya (Devil)"},
    {117, "Jin (Angel)"},
    {118, "Kazuya (True Devil)"},
    {121, "Jin (Devil)"},
    {244, "Kazuya (Final)"},
    {351, "Heihachi (Monk)"},
    {352, "Heihachi (Shadow)"},
    {353, "Heihachi (Final)"},
};

// Function declarations
void InitializeUI(HWND hwnd);
void PopulateComboBox(HWND comboBox);
void AppendLog(const std::string &msg);
void AttachToGame(); // TODO: Implement game attachment logic
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void HandleBossSelection()
{
  // TODO: Ensure game is attached before modifying memory

  int idx1 = SendMessageA(hwndCombo1, CB_GETCURSEL, 0, 0);
  int idx2 = SendMessageA(hwndCombo2, CB_GETCURSEL, 0, 0);

  if (idx1 >= 0 && idx1 < bossList.size() && idx2 >= 0 && idx2 < bossList.size())
  {
    std::string logMsg = "Player 1: " + std::string(bossList[idx1].name) +
                         " | Player 2: " + std::string(bossList[idx2].name);
    AppendLog(logMsg);

    // TODO: Implement game-hacking logic (WriteProcessMemory, etc.)
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

  // TODO: Call AttachToGame() in a separate thread if needed
  AttachToGame();

  MSG msg = {};
  while (GetMessageA(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  return 0;
}

// Initialize UI components
void InitializeUI(HWND hwnd)
{
  const int padding = 20;
  const int comboWidth = 180;
  const int comboHeight = 200;
  const int buttonWidth = 120;
  const int buttonHeight = 30;
  const int logWidth = 440;
  const int logHeight = 100;

  hwndCombo1 = CreateWindowA("COMBOBOX", NULL,
                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             padding, padding, comboWidth, comboHeight, hwnd, (HMENU)1, NULL, NULL);

  hwndCombo2 = CreateWindowA("COMBOBOX", NULL,
                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             padding + comboWidth + 20, padding, comboWidth, comboHeight, hwnd, (HMENU)2, NULL, NULL);

  hwndButton = CreateWindowA("BUTTON", "Apply Selection",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             (500 - buttonWidth) / 2, padding + 50, buttonWidth, buttonHeight, hwnd, (HMENU)3, NULL, NULL);

  hwndLogBox = CreateWindowA("EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                             padding, 150, logWidth, logHeight, hwnd, NULL, NULL, NULL);

  PopulateComboBox(hwndCombo1);
  PopulateComboBox(hwndCombo2);

  SendMessageA(hwndCombo1, CB_SETCURSEL, 0, 0);
  SendMessageA(hwndCombo2, CB_SETCURSEL, 0, 0);
}

// Populate a ComboBox with boss names
void PopulateComboBox(HWND comboBox)
{
  for (const auto &boss : bossList)
  {
    SendMessageA(comboBox, CB_ADDSTRING, 0, (LPARAM)boss.name);
  }
}

// Append message to log box
void AppendLog(const std::string &msg)
{
  int length = GetWindowTextLengthA(hwndLogBox);
  SendMessageA(hwndLogBox, EM_SETSEL, length, length);
  SendMessageA(hwndLogBox, EM_REPLACESEL, 0, (LPARAM)(msg + "\r\n").c_str());
}

// TODO: Implement this function to attach to the game
void AttachToGame()
{
  char buffer[255];
  AppendLog("Waiting for game to start...");
  // TODO: Implement process scanning and attachment logic
  if (game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    AppendLog("Successfully attached to game!");
    sprintf_s(buffer, "Base Address: 0x%llx", game.getBaseAddress());
    AppendLog(buffer);
    // TODO: Call address scanner or other game-hacking methods
  }
}

// Window Procedure (Handles UI Events)
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
  case WM_COMMAND:
    if (LOWORD(wp) == 3) // Apply Selection button clicked
    {
      HandleBossSelection();
    }
    break;
  case WM_DESTROY:
    // TODO: Close any open handles to the game process if necessary
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProcA(hwnd, msg, wp, lp);
  }
  return 0;
}
