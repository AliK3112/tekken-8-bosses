#include <conio.h>
#include "bosses.h"

bool _DEV_MODE = 1;

void mainFunc(TkBossLoader &bossLoader, int bossCode, int selectedSide);
int getSideSelection();
int takeInput();

int main()
{
  int bossCode = _DEV_MODE ? BossCodes::DevilJin : -1;
  TkBossLoader bossLoader;

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

  int selectedSide = _DEV_MODE ? 0 : getSideSelection();
  if (selectedSide == -1)
    return 0;


  bossCode = _DEV_MODE ? bossCode : takeInput();

  if (bossCode != -1)
    mainFunc(bossLoader, bossCode, selectedSide);

  printf("Press any key to close the script\n");
  _getch();
  return 0;
}

void mainFunc(TkBossLoader &bossLoader, int bossCode, int selectedSide)
{
  bossLoader.setBossCodeForSelectedSide(selectedSide, bossCode);
  bossLoader.bossLoadMainLoop(selectedSide);
}

int getSideSelection()
{
  int selectedSide = 0;
  while (true)
  {
    std::cout << "\nWhich side do you want to activate this script for?\n";
    std::cout << "For Left side press 'L' or '0'\n";
    std::cout << "For Right side press 'R' or '1'\n";
    std::cout << "For exiting: Esc or Q\n";
    char input = _getch();

    if (input == '0' || input == '1')
    {
      selectedSide = input - '0';
      break;
    }
    else if (input == 'L' || input == 'l' || input == 'R' || input == 'r')
    {
      selectedSide = (input | 0x20) == 'r';
      break;
    }
    else if (input == 27 || input == 'q' || input == 'Q')
    {
      return -1;
    }
    else
    {
      std::cout << "\nInvalid input! Please press 'L', 'R', '0' or '1'\n";
    }
  }
  return selectedSide;
}

int takeInput()
{
  printf("**NOTE**, Eligible Game Modes: Practice, Arcade, Story, Versus and Tekken Ball\n");
  printf("\nSelect the Boss that you want to play as\n");
  printf("1. Boosted Jin from Chapter 1\n");
  printf("2. Nerfed Jin\n");
  printf("3. Chained Jin from Chapter 12 Battle 3\n");
  printf("4. Mishima Jin from Chapter 15 Battle 2\n");
  printf("5. Kazama Jin from Chapter 15 Battle 3\n");
  printf("6. Awakened Jin from Chapter 15 Final Battle\n");
  printf("7. Devil Kazuya from Chapter 6\n");
  printf("8. Final Battle Kazuya\n");
  printf("9. Monk/Amnesia Heihachi from Story DLC\n");
  printf("A. Shadow Heihachi from Story DLC\n");
  printf("B. Heihachi from Story DLC Finale\n");
  printf("C. Angel Jin\n");
  printf("D. True Devil Kazuya\n");
  printf("E. Story Devil Jin\n");
  printf("F. Azazel\n");
  printf("Press any other key to exit\n");
  int input = _getch();
  switch (input)
  {
  case '1':
    return BossCodes::RegularJin;
  case '2':
    return BossCodes::NerfedJin;
  case '3':
    return BossCodes::ChainedJin;
  case '4':
    return BossCodes::MishimaJin;
  case '5':
    return BossCodes::KazamaJin;
  case '6':
    return BossCodes::FinalJin;
  case '7':
    return BossCodes::DevilKazuya;
  case '8':
    return BossCodes::FinalKazuya;
  case '9':
    return BossCodes::AmnesiaHeihachi;
  case 'A':
  case 'a':
    return BossCodes::ShadowHeihachi;
  case 'B':
  case 'b':
    return BossCodes::FinalHeihachi;
  case 'C':
  case 'c':
    return BossCodes::AngelJin;
  case 'D':
  case 'd':
    return BossCodes::TrueDevilKazuya;
  case 'E':
  case 'e':
    return BossCodes::DevilJin;
  case 'F':
  case 'f':
    return BossCodes::Azazel;
  default:
    return -1;
  }
  return -1;
}
