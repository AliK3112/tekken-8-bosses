#include <windows.h>
#include <vector>
#include <string>
#include <cstdarg>
#include "bosses.h"
#include "resource.h"

const char CLASS_NAME[] = "BossSelectorWindow";

// Global UI elements
HWND hwndLabel1, hwndLabel2, hwndCombo1, hwndCombo2, hwndLogBox, hwndCheckbox, hwndCheckParry, hwndCheckDamage, hwndCheckRage;
TkBossLoader boss;
ConfigFlags config;
char buffer[255];

// Boss mapping
struct Boss
{
  int id;
  const char *name;
};

std::vector<Boss> bossList = {
    {BossCodes::None, "None"},
    {BossCodes::RegularJin, "Jin - Boosted"},
    {BossCodes::NerfedJin, "Jin - Karate"},
    {BossCodes::MishimaJin, "Jin - Mishima"},
    {BossCodes::KazamaJin, "Jin - Kazama"},
    {BossCodes::FinalJin, "Jin - Final"},
    {BossCodes::ChainedJin, "Jin - Chained"},
    {BossCodes::AngelJin, "Angel Jin"},
    // {BossCodes::DevilJin, "Jin - Devil"},
    {BossCodes::DevilJin_1, "Devil Jin - Chapter 1"},
    {BossCodes::DevilJin_2, "Devil Jin - Chapter 12"},
    {BossCodes::DevilJin_3, "Devil Jin - Chapter 13"},
    {BossCodes::FinalKazuya, "Kazuya - Final"},
    {BossCodes::DevilKazuya, "Kazuya - Devil"},
    {BossCodes::TrueDevilKazuya, "Kazuya - True Devil"},
    {BossCodes::AmnesiaHeihachi, "Heihachi - Monk"},
    {BossCodes::ShadowHeihachi, "Heihachi - Shadow"},
    {BossCodes::FinalHeihachi, "Heihachi - Final"},
    {BossCodes::Azazel, "Azazel"},
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

void SaveConfig()
{
  WritePrivateProfileStringA("Settings", "LoadHudAndCostumes", config.handleHudAndCostumes ? "1" : "0", ".\\boss_config.ini");
  WritePrivateProfileStringA("Settings", "DisableAutoParries", config.disableAutoParries ? "1" : "0", ".\\boss_config.ini");
  WritePrivateProfileStringA("Settings", "ToneDownDamage", config.toneDownDamage ? "1" : "0", ".\\boss_config.ini");
  WritePrivateProfileStringA("Settings", "FinalKazuyaRageBlast", config.finalKazuyaRageBlast ? "1" : "0", ".\\boss_config.ini");
}

void LoadConfig()
{
  config.handleHudAndCostumes = GetPrivateProfileIntA("Settings", "LoadHudAndCostumes", 1, ".\\boss_config.ini");
  config.disableAutoParries = GetPrivateProfileIntA("Settings", "DisableAutoParries", 0, ".\\boss_config.ini");
  config.toneDownDamage = GetPrivateProfileIntA("Settings", "ToneDownDamage", 0, ".\\boss_config.ini");
  config.finalKazuyaRageBlast = GetPrivateProfileIntA("Settings", "FinalKazuyaRageBlast", 1, ".\\boss_config.ini");
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
  WNDCLASSA wc = {};
  wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MYICON));
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(CLASS_NAME, "TEKKEN 8 - Boss Selector",
                            WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 500, 530,
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
  // Layout Constants
  const int MARGIN_X = 20;
  const int MARGIN_Y = 20;
  const int SPACING_Y = 10;       // Vertical spacing between related controls
  const int GROUP_SPACING = 25;   // Spacing between major sections
  const int LABEL_HEIGHT = 18;
  const int COMBO_HEIGHT = 300;   // Includes dropdown list height
  const int CHECKBOX_HEIGHT = 20;
  const int SUBTEXT_HEIGHT = 16;
  
  // Get Client Area Dimensions
  RECT clientRect;
  GetClientRect(hwnd, &clientRect);
  int clientWidth = clientRect.right - clientRect.left;
  int clientHeight = clientRect.bottom - clientRect.top;

  // If client rect is not yet initialized (edge case), fallback to expected size
  if (clientWidth == 0) clientWidth = 484; // Approx client width for 500px window
  
  int contentWidth = clientWidth - (2 * MARGIN_X);
  int halfWidth = (contentWidth - SPACING_Y) / 2;
  int currentY = MARGIN_Y;

  // --- 1. Boss Selection Section ---
  
  // Player 1 Header
  hwndLabel1 = CreateWindowA("STATIC", "Player 1 Character", WS_CHILD | WS_VISIBLE | SS_LEFT, 
      MARGIN_X, currentY, halfWidth, LABEL_HEIGHT, hwnd, NULL, NULL, NULL);
      
  // Player 2 Header
  hwndLabel2 = CreateWindowA("STATIC", "Player 2 Character", WS_CHILD | WS_VISIBLE | SS_LEFT, 
      MARGIN_X + halfWidth + SPACING_Y, currentY, halfWidth, LABEL_HEIGHT, hwnd, NULL, NULL, NULL);
      
  currentY += LABEL_HEIGHT + 5; // Gap between label and combo

  // Player 1 Combo
  hwndCombo1 = CreateWindowA("COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
      MARGIN_X, currentY, halfWidth, COMBO_HEIGHT, hwnd, (HMENU)1, NULL, NULL);

  // Player 2 Combo
  hwndCombo2 = CreateWindowA("COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
      MARGIN_X + halfWidth + SPACING_Y, currentY, halfWidth, COMBO_HEIGHT, hwnd, (HMENU)2, NULL, NULL);

  currentY += 25 + GROUP_SPACING; // Height of closed combo (approx 25) + spacing

  // --- 2. Configuration Section (Group Box) ---

  // Calculate Group Box Height based on contents
  // 4 checkboxes (20px) + 4 subtexts (16px) + spacings
  int groupContentHeight = (4 * (CHECKBOX_HEIGHT + SUBTEXT_HEIGHT + 2)) + 20; 
  int groupTotalHeight = groupContentHeight + 20; 

  HWND hwndGroupObj = CreateWindowA("BUTTON", "Configuration", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
      MARGIN_X, currentY, contentWidth, groupTotalHeight, hwnd, NULL, NULL, NULL);

  // Group Content Coordinates
  int groupInnerX = MARGIN_X + 15;
  int groupInnerW = contentWidth - 30;
  int groupCursorY = currentY + 20; // Start below group title

  // Item 1
  hwndCheckbox = CreateWindowA("BUTTON", "Load HUD and Costume for unique bosses", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      groupInnerX, groupCursorY, groupInnerW, CHECKBOX_HEIGHT, hwnd, (HMENU)3, NULL, NULL);
  groupCursorY += CHECKBOX_HEIGHT;
  
  CreateWindowA("STATIC", "  (Excludes: Angel Jin, True Devil Kazuya and Story Devil Jin)", WS_CHILD | WS_VISIBLE | SS_LEFT,
      groupInnerX, groupCursorY, groupInnerW, SUBTEXT_HEIGHT, hwnd, NULL, NULL, NULL);
  groupCursorY += SUBTEXT_HEIGHT + 5;

  // Item 2
  hwndCheckParry = CreateWindowA("BUTTON", "Disable Auto-Parries", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      groupInnerX, groupCursorY, groupInnerW, CHECKBOX_HEIGHT, hwnd, (HMENU)4, NULL, NULL);
  groupCursorY += CHECKBOX_HEIGHT;

  CreateWindowA("STATIC", "  (Applies to: Jin and Heihachi Final variants)", WS_CHILD | WS_VISIBLE | SS_LEFT,
      groupInnerX, groupCursorY, groupInnerW, SUBTEXT_HEIGHT, hwnd, NULL, NULL, NULL);
  groupCursorY += SUBTEXT_HEIGHT + 5;

  // Item 3
  hwndCheckDamage = CreateWindowA("BUTTON", "Tone Down Excessive Damage", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      groupInnerX, groupCursorY, groupInnerW, CHECKBOX_HEIGHT, hwnd, (HMENU)5, NULL, NULL);
  groupCursorY += CHECKBOX_HEIGHT;

  CreateWindowA("STATIC", "  (Applies to: Final Kazuya ws+2, Angel Jin CD+1)", WS_CHILD | WS_VISIBLE | SS_LEFT,
      groupInnerX, groupCursorY, groupInnerW, SUBTEXT_HEIGHT, hwnd, NULL, NULL, NULL);
  groupCursorY += SUBTEXT_HEIGHT + 5;

  // Item 4
  hwndCheckRage = CreateWindowA("BUTTON", "Final Battle Kazuya Rage Art", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      groupInnerX, groupCursorY, groupInnerW, CHECKBOX_HEIGHT, hwnd, (HMENU)6, NULL, NULL);
  groupCursorY += CHECKBOX_HEIGHT;

  CreateWindowA("STATIC", "  (Set \"Rage Blast\" as his \"Rage Art\")", WS_CHILD | WS_VISIBLE | SS_LEFT,
      groupInnerX, groupCursorY, groupInnerW, SUBTEXT_HEIGHT, hwnd, NULL, NULL, NULL);

  currentY += groupTotalHeight + GROUP_SPACING;

  // --- 3. Log Section ---

  // Instructions Labels
  CreateWindowA("STATIC", "Note 1: Reload match after changing settings to apply.", WS_CHILD | WS_VISIBLE | SS_LEFT, 
      MARGIN_X, currentY, contentWidth, LABEL_HEIGHT, hwnd, NULL, NULL, NULL);
  currentY += LABEL_HEIGHT + 2;

  CreateWindowA("STATIC", "Note 2: Story Rage Art cameras require a match reload to work.", WS_CHILD | WS_VISIBLE | SS_LEFT,
      MARGIN_X, currentY, contentWidth, LABEL_HEIGHT, hwnd, NULL, NULL, NULL);
  currentY += LABEL_HEIGHT + 5;

  // Log Box fills remaining space (with margin at bottom)
  // Check if we have vertical space left, otherwise give it a fixed minimum
  int remainingHeight = clientHeight - currentY - MARGIN_Y;
  if (remainingHeight < 60) remainingHeight = 80; // Minimum usable height

  hwndLogBox = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
      MARGIN_X, currentY, contentWidth, remainingHeight, hwnd, NULL, NULL, NULL);

  // --- Initialization ---
  PopulateComboBox(hwndCombo1);
  PopulateComboBox(hwndCombo2);

  SendMessageA(hwndCombo1, CB_SETCURSEL, 0, 0);
  SendMessageA(hwndCombo2, CB_SETCURSEL, 0, 0);

  LoadConfig();
  boss.setConfig(&config);

  SendMessageA(hwndCheckbox, BM_SETCHECK, config.handleHudAndCostumes ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(hwndCheckParry, BM_SETCHECK, config.disableAutoParries ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(hwndCheckDamage, BM_SETCHECK, config.toneDownDamage ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(hwndCheckRage, BM_SETCHECK, config.finalKazuyaRageBlast ? BST_CHECKED : BST_UNCHECKED, 0);
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

  EnableWindow(hwndCombo1, FALSE);
  EnableWindow(hwndCombo2, FALSE);

  while (true)
  {
    if (boss.attach())
    {
      AppendLog("Successfully attached to game!");
      // AppendLog(buffer, "Base Address: 0x%llx", boss.game.getBaseAddress());
      break;
    }
    Sleep(1000);
  }

  boss.attachToLogBox(hwndLogBox);

  AppendLog("Scanning for addresses...\n");
  boss.scanForAddresses();
  AppendLog("Addresses successfully scanned...\n");

  if (boss.isReady())
  {
    EnableWindow(hwndCombo1, TRUE);
    EnableWindow(hwndCombo2, TRUE);

    boss.bossLoadMainLoop();
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
  case WM_COMMAND:
  {
    int controlId = LOWORD(wp);
    int notificationCode = HIWORD(wp);

    if (notificationCode == CBN_SELCHANGE && (controlId == 1 || controlId == 2))
    {
      HandleBossSelection();
    }
    if (notificationCode == BN_CLICKED)
    {
      if (controlId == 3)
      {
        config.handleHudAndCostumes = (SendMessageA(hwndCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SaveConfig();
      }
      else if (controlId == 4)
      {
        config.disableAutoParries = (SendMessageA(hwndCheckParry, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SaveConfig();
      }
      else if (controlId == 5)
      {
        config.toneDownDamage = (SendMessageA(hwndCheckDamage, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SaveConfig();
      }
      else if (controlId == 6)
      {
        config.finalKazuyaRageBlast = (SendMessageA(hwndCheckRage, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SaveConfig();
      }
    }
  }
  break;
  case WM_CTLCOLORSTATIC:
  {
    if ((HWND)lp == hwndLogBox)
    {
      HDC hdc = (HDC)wp;
      SetBkColor(hdc, RGB(255, 255, 255));
      SetTextColor(hdc, RGB(0, 0, 0));
      return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
  }
  break;
  case WM_DESTROY:
    boss.uninstallStoryCameraHook();
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
