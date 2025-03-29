#include <windows.h>
#include <vector>
#include <string>

// #include "game_hacking.h" // Include your game hacking logic here

const char CLASS_NAME[] = "BossSelectorWindow";

// Global UI elements
HWND hwndCombo1, hwndCombo2, hwndButton;

// Boss mapping (C++ equivalent of the Python dictionary)
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
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void HandleBossSelection()
{
  // Placeholder: Retrieve selected values
  int idx1 = SendMessageA(hwndCombo1, CB_GETCURSEL, 0, 0);
  int idx2 = SendMessageA(hwndCombo2, CB_GETCURSEL, 0, 0);

  if (idx1 >= 0 && idx1 < bossList.size() && idx2 >= 0 && idx2 < bossList.size())
  {
    std::string message = "Player 1: " + std::string(bossList[idx1].name) + "\n" +
                          "Player 2: " + std::string(bossList[idx2].name);
    MessageBoxA(NULL, message.c_str(), "Selected Bosses", MB_OK);
  }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
  WNDCLASSA wc = {}; // Use the ANSI version explicitly
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(CLASS_NAME, "Boss Selector",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
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

// Initialize UI components
void InitializeUI(HWND hwnd)
{
  hwndCombo1 = CreateWindowA("COMBOBOX", NULL,
                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             20, 20, 150, 200, hwnd, (HMENU)1, NULL, NULL);

  hwndCombo2 = CreateWindowA("COMBOBOX", NULL,
                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                             200, 20, 150, 200, hwnd, (HMENU)2, NULL, NULL);

  hwndButton = CreateWindowA("BUTTON", "Apply Selection",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             130, 80, 120, 30, hwnd, (HMENU)3, NULL, NULL);

  // Populate dropdowns with boss names
  PopulateComboBox(hwndCombo1);
  PopulateComboBox(hwndCombo2);

  // Set default selection to the first item
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

// Window Procedure (Handles UI Events)
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
  case WM_COMMAND:
    if (LOWORD(wp) == 3) // Button Clicked
    {
      HandleBossSelection(); // Trigger game logic when button is clicked
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
