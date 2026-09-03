// DEBUGGING SCRIPT

#include <conio.h>
#include "bosses.h"

static TkBossLoader *g_bossLoader = nullptr;

enum PlayerSide {
  Left = 0,
  Right = 1,
};

BOOL WINAPI ConsoleCtrlHandler(DWORD signal)
{
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT)
  {
    if (g_bossLoader)
    {
      g_bossLoader->uninstallStoryCameraHook();
      g_bossLoader->uninstallDramaCameraHook();
    }
    return FALSE; // let the process terminate
  }
  return FALSE;
}

int main()
{
  int bossCode = BossCodes::AngelJin;
  int selectedSide = PlayerSide::Left;
  ConfigFlags config = {.disableAutoParries = false, .handleHudAndCostumes = true, .toneDownDamage = false};
  TkBossLoader bossLoader;
  bossLoader.setConfig(&config);
  g_bossLoader = &bossLoader;
  SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

  bossLoader.setDevModeFlag(true);

  printf("Waiting for Tekken 8 to run...\n");
  while (true)
  {
    if (bossLoader.attach())
    {
      break;
    }
    Sleep(1000);
  }

  bossLoader.scanForAddresses();

  if (bossCode != -1)
  {
    bossLoader.setBossCodeForSelectedSide(selectedSide, bossCode);
    bossLoader.bossLoadMainLoop(selectedSide);
  }

  // printf("Press any key to close the script\n");
  // _getch();
  return 0;
}
