#include "Game.h"
#include "utils.h"
#include "tekken.h"
#include <conio.h>
#include <unistd.h>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <csignal>

using namespace Tekken;

// Globals
GameClass Game;
std::map<std::string, uintptr_t> addresses;
uintptr_t MOVESET_OFFSET = 0;
uintptr_t DECRYPT_FUNC_ADDR = 0;
uintptr_t PERMA_DEVIL_OFFSET = 0;
uintptr_t PLAYER_STRUCT_BASE = 0;
uintptr_t MATCH_STRUCT_OFFSET = 0;
uintptr_t HUD_ICON_ADDR = 0;
uintptr_t HUD_NAME_ADDR = 0;

std::string FINAL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_naked.CS_ant_1p_naked";
std::string CHAINED_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_chain.CS_ant_1p_chain";
std::string FINAL_KAZ_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_grl_1p_v2_white.CS_grl_1p_v2_white";
std::string DEVIL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_swl_ant_1p.CS_swl_ant_1p";
std::string HEIHACHI_MONK_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_whitetiger_nohat_nomask.CS_bee_whitetiger_nohat_nomask";
std::string HEIHACHI_SHADOW_COSTUME_PATH = "/Game/Demo/Ingame/Item/Sets/CS_bee_1p_p_shadow.CS_bee_1p_p_shadow";

bool DEV_MODE = false;
bool HANDLE_ICONS = false;
int STORY_FLAGS_REQ = 777;
int STORY_BATTLE_REQ = 668;
int END_REQ = 1100;
int SIDE_SELECTED = 0;
std::string BOSS_NAME;

std::vector<int> STORY_REQS = {
  667, // Story Fight
  668, // Story Battle Number
  777, // Story Flags
  801, // (DLC) Story Fight
  802, // (DLC) Story Battle Number
  806, // (DLC) Story Flags
};

struct EncryptedValue
{
  uintptr_t value;
  uintptr_t key;
};

void storeAddresses();
void scanAddresses();
int getSideSelection();
void mainFunc(int bossCode);
int takeInput();
void loadCostume(uintptr_t matchStructAddr, int costumeId, std::string costumePath);
void costumeHandler(uintptr_t matchStructAddr, int bossCode);
void hudHandler(uintptr_t matchStructAddr, int bossCode);
void sleep(int ms) { usleep(ms * 1000); }
bool loadBoss(uintptr_t playerAddr, uintptr_t moveset, int bossCode);
void loadCharacter(uintptr_t matchStructAddr, int bossCode);
bool loadJin(uintptr_t moveset, int bossCode);
bool loadKazuya(uintptr_t moveset, int bossCode);
bool loadAzazel(uintptr_t moveset, int bossCode);
bool loadHeihachi(uintptr_t moveset, int bossCode);
bool loadAngelJin(uintptr_t moveset, int bossCode);
bool loadTrueDevilKazuya(uintptr_t moveset, int bossCode);
bool loadStoryDevilJin(uintptr_t moveset, int bossCode);
uintptr_t getMoveAddress(uintptr_t moveset, int moveNameKey, int start);
uintptr_t getMoveAddressByIdx(uintptr_t moveset, int idx);
int getMoveId(uintptr_t moveset, int moveNameKey, int start);
bool funcAddrIsValid(uintptr_t funcAddr);
bool movesetExists(uintptr_t moveset);
bool isMovesetEdited(uintptr_t moveset);
bool isEligible(uintptr_t matchStruct);
bool cancelHasCondition(uintptr_t cancel, int req, int param);
void adjustIntroOutroReq(uintptr_t moveset, int bossCode, int start);
uintptr_t getCancelReqAddr(uintptr_t cancel);
uintptr_t getMoveNthCancel(uintptr_t move, int n);
uintptr_t getMoveNthCancel1stReqAddr(uintptr_t move, int n);
uintptr_t getNthCancelFlagAddr(uintptr_t moveset, int n);
bool markMovesetEdited(uintptr_t moveset);
void modifyHudAddr(uintptr_t matchStructAddr);
void restoreHudAddr(uintptr_t matchStructAddr);
void signalHandler(int signal);
BOOL WINAPI ConsoleHandler(DWORD signal);
void atExit();

int main()
{
  int bossCode = DEV_MODE ? BossCodes::DevilKazuya : -1;
  // Set up end-program handler
  if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
	{
		std::cerr << "Error: Could not set control handler\n";
		return 1;
	}
	std::atexit(atExit);
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
  //
  printf("Waiting for Tekken 8 to run...\n");
  while (true)
  {
    if (Game.Attach(L"Polaris-Win64-Shipping.exe"))
    {
      break;
    }
    sleep(1000);
  }

  scanAddresses();

  if (!DEV_MODE)
    SIDE_SELECTED = getSideSelection();
  if (SIDE_SELECTED == -1)
  {
    return 0;
  }
  if (DEV_MODE)
  {
    addresses = readKeyValuePairs("C:\\Users\\alikh\\Documents\\Projects\\tekken-8-bosses\\addresses.txt");
  }
  else
  {
    addresses = readKeyValuePairs("addresses.txt");
  }
  storeAddresses();

  // Validating the function address
  if (!funcAddrIsValid(DECRYPT_FUNC_ADDR))
  {
    printf("Function address is invalid. The script will not be able to work if this is not correct.\nPress any key to close the script\n");
    _getch();
    return 0;
  }

  if (!DEV_MODE)
    bossCode = takeInput();
  if (bossCode != -1)
  {
    mainFunc(bossCode);
  }

  printf("Press any key to close the script\n");
  _getch();
  return 0;
}

void storeAddresses()
{
  uintptr_t gameBaseAddr = Game.getBaseAddress();

  PERMA_DEVIL_OFFSET = getValueByKey(addresses, "permanent_devil_offset");
  PLAYER_STRUCT_BASE = getValueByKey(addresses, "player_struct_base");
  MATCH_STRUCT_OFFSET = getValueByKey(addresses, "match_struct");
}

void scanAddresses()
{
  uintptr_t addr = Game.FastAoBScan(Tekken::ENC_SIG_BYTES);
  if (addr != 0) {
    DECRYPT_FUNC_ADDR = addr;
  } else {
    throw std::runtime_error("Decryption Function Address not found!");
  }

  addr = Game.FastAoBScan(Tekken::HUD_ICON_SIG_BYTES, Game.getBaseAddress() + 0x5A00000);
  HUD_ICON_ADDR = addr + 13;

  addr = Game.FastAoBScan(Tekken::HUD_NAME_SIG_BYTES, addr + 0x10, addr + 0x1000);
  HUD_NAME_ADDR = addr + 13;

  // Setting the global flag
  HANDLE_ICONS = HUD_ICON_ADDR && HUD_NAME_ADDR;

  addr = Game.FastAoBScan(Tekken::MOVSET_OFFSET_SIG_BYTES, DECRYPT_FUNC_ADDR + 0x1000);
  if (addr != 0) {
    MOVESET_OFFSET = Game.readUInt32(addr + 3);
  } else {
    throw std::runtime_error("Moveset Offset not found!");
  }

  if (DEV_MODE) {
    printf("DECRYPT_FUNC_ADDR: 0x%x\n", DECRYPT_FUNC_ADDR);
    printf("HUD_ICON_ADDR: 0x%x\n", HUD_ICON_ADDR);
    printf("HUD_NAME_ADDR: 0x%x\n", HUD_NAME_ADDR);
    printf("MOVESET_OFFSET: 0x%x\n", MOVESET_OFFSET);
  }
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

void mainFunc(int bossCode)
{
  // system("cls");
  BOSS_NAME = getBossName(bossCode);
  printf("Please load into the game with the appropriate character, the Game will automatically detect & load the altered moveset\n");
  bool isWritten = false;
  bool flag = false;
  uintptr_t matchStructAddr = Game.getAddress({(DWORD)MATCH_STRUCT_OFFSET, 0x50, 0x8, 0x18, 0x8});
  if (!matchStructAddr)
  {
    printf("Cannot find the match structure address.\n");
    return;
  }
  uintptr_t lastAddr = 0;

  while (true)
  {
    sleep(100);
    matchStructAddr = Game.getAddress({(DWORD)MATCH_STRUCT_OFFSET, 0x50, 0x8, 0x18, 0x8});
    if (matchStructAddr == 0)
    {
      continue;
    }

    // At this point, the matchStruct is present, so modify the instructions
    if (HANDLE_ICONS) modifyHudAddr(matchStructAddr);

    if (!isEligible(matchStructAddr))
    {
      continue;
    }

    // Set Character ID
    loadCharacter(matchStructAddr, bossCode);

    // Handle HUD icons
    if (HANDLE_ICONS) hudHandler(matchStructAddr, bossCode);

    uintptr_t playerAddr = Game.getAddress({(DWORD)PLAYER_STRUCT_BASE, (DWORD)(0x30 + SIDE_SELECTED * 8)});
    if (playerAddr == 0)
    {
      costumeHandler(matchStructAddr, bossCode);
      continue;
    }

    uintptr_t movesetAddr = Game.ReadUnsignedLong(playerAddr + MOVESET_OFFSET);
    if (movesetAddr == 0)
    {
      flag = false;
      isWritten = false;
      continue;
    }

    // At this point, the moveset is loaded, so restore the instructions
    if (HANDLE_ICONS) restoreHudAddr(matchStructAddr);

    if (!movesetExists(movesetAddr))
    {
      isWritten = false;
      continue;
    }

    if (!isMovesetEdited(movesetAddr))
    {
      isWritten = false;
    }

    if (!isWritten)
    {
      isWritten = loadBoss(playerAddr, movesetAddr, bossCode);
      if (!flag && isWritten)
        printf("Boss loaded: %s\n", BOSS_NAME.c_str());
      flag = true;
    }
    // break;
  }
}

void loadCostume(uintptr_t matchStructAddr, int costumeId, std::string costumePath)
{
  Game.write<int>(matchStructAddr + 0x6F0 + SIDE_SELECTED * 0x6760, costumeId);
  Game.writeString(matchStructAddr + 0x13D78 + SIDE_SELECTED * 0x100, costumePath);
}

void costumeHandler(uintptr_t matchStructAddr, int bossCode)
{
  if (!matchStructAddr)
    return;
  std::string costumePath;
  switch (bossCode)
  {
  case BossCodes::RegularJin:
  case BossCodes::NerfedJin:
  case BossCodes::DevilKazuya:
  case BossCodes::FinalHeihachi:
  case BossCodes::Azazel:
  case BossCodes::AngelJin:
  case BossCodes::TrueDevilKazuya:
    return loadCostume(matchStructAddr, 0, "");
  case BossCodes::ChainedJin:
    costumePath = CHAINED_JIN_COSTUME_PATH;
    break;
  case BossCodes::MishimaJin:
  case BossCodes::KazamaJin:
  case BossCodes::FinalJin:
    costumePath = FINAL_JIN_COSTUME_PATH;
    break;
  case BossCodes::FinalKazuya:
    costumePath = FINAL_KAZ_COSTUME_PATH;
    break;
  case BossCodes::DevilJin:
    costumePath = DEVIL_JIN_COSTUME_PATH;
    break;
  case BossCodes::AmnesiaHeihachi:
    costumePath = HEIHACHI_MONK_COSTUME_PATH;
    break;
  case BossCodes::ShadowHeihachi:
    costumePath = HEIHACHI_SHADOW_COSTUME_PATH;
    break;
  default:
    return;
  }
  loadCostume(matchStructAddr, 51, costumePath);
}

void loadBossHud(uintptr_t matchStruct, int side, int charId, int bossCode)
{
  std::string icon;
  std::string name;
  const char c = side == 0 ? 'L' : 'R';
  if (bossCode == BossCodes::DevilJin && charId == BossCodes::DevilJin)
  {
    icon = buildString(c, getCharCode(6));
    name = getNamePath(6);
  }
  else if ((bossCode == BossCodes::FinalJin || bossCode == BossCodes::MishimaJin || bossCode == BossCodes::KazamaJin) && charId == 6)
  {
    icon = buildString(c, "ant2");
    name = getNamePath(6);
  }
  else if (bossCode == BossCodes::FinalKazuya && charId == 8)
  {
    icon = buildString(c, "grl2");
    name = getNamePath(8);
  }
  else if (bossCode == BossCodes::DevilKazuya && charId == 8)
  {
    icon = buildString(c, "grl3");
    name = getNamePath("grl2");
  }
  else if (bossCode == BossCodes::AmnesiaHeihachi && charId == 35)
  {
    icon = buildString(c, "bee2");
    name = getNamePath(35);
  }
  else if (bossCode == BossCodes::ShadowHeihachi && charId == 35)
  {
    icon = buildString(c, "bee3");
    name = getNamePath("bee3");
  }
  if (!icon.empty()) Game.writeString(matchStruct + 0x2C0 + side * 0x100, icon);
  if (!name.empty()) Game.writeString(matchStruct + 0x4C0 + side * 0x100, name);
}

void hudHandler(uintptr_t matchStruct, int bossCode)
{
  int char1 = Game.readInt32(matchStruct + 0x10);
  int char2 = Game.readInt32(matchStruct + 0x94);
  std::string icon1 = getIconPath(0, char1);
  std::string icon2 = getIconPath(1, char2);
  std::string name1 = getNamePath(char1);
  std::string name2 = getNamePath(char2);
  Game.writeString(matchStruct + 0x2C0, icon1);
  Game.writeString(matchStruct + 0x3C0, icon2);
  Game.writeString(matchStruct + 0x4C0, name1);
  Game.writeString(matchStruct + 0x5C0, name2);

  loadBossHud(matchStruct, SIDE_SELECTED, SIDE_SELECTED ? char2 : char1, bossCode);
}

void loadCharacter(uintptr_t matchStructAddr, int bossCode)
{
  int charId = -1;
  int currCharId = Game.readInt32(matchStructAddr + 0x10 + SIDE_SELECTED * 0x84);
  switch (bossCode)
  {
  // In these cases, the bossCode is the chara ID itself
  case BossCodes::Azazel:
  case BossCodes::AngelJin:
  case BossCodes::TrueDevilKazuya:
  case BossCodes::DevilJin:
    charId = bossCode;
    break;
  default:
    return;
  }
  if (charId != -1 && isEligible(matchStructAddr))
  {
    Game.write<int>(matchStructAddr + 0x10 + SIDE_SELECTED * 0x84, charId);
    if (charId == BossCodes::DevilJin)
    {
      loadCostume(matchStructAddr, 51, DEVIL_JIN_COSTUME_PATH); // Just a safety precaution
    }
  }
}

bool disableRequirements(uintptr_t moveset, int targetReq, int targetParam)
{
  uintptr_t requirements = Game.ReadUnsignedLong(moveset + Offsets::Moveset::RequirementsHeader);
  int requirementsCount = Game.ReadSignedInt(moveset + Offsets::Moveset::RequirementsCount);
  for (int i = 0; i < requirementsCount; i++)
  {
    uintptr_t addr = requirements + i * Sizes::Requirement;
    int req = Game.ReadSignedInt(addr);
    int param = Game.ReadSignedInt(addr + 4);
    if (req == targetReq && param == targetParam)
    {
      Game.write<uint64_t>(addr, 0);
    }
  }
  return true;
}

bool loadBoss(uintptr_t playerAddr, uintptr_t moveset, int bossCode)
{
  int charId = Game.readInt32(playerAddr + 0x168);

  switch (charId)
  {
  case 6: return loadJin(moveset, bossCode);
  case 8:
  {
    if (bossCode == BossCodes::DevilKazuya)
    {
      Game.write<int>(playerAddr + PERMA_DEVIL_OFFSET, 1);
    }
    return loadKazuya(moveset, bossCode);
  }
  case 32: return loadAzazel(moveset, bossCode);
  case 35: return loadHeihachi(moveset, bossCode);
  case 117: return loadAngelJin(moveset, bossCode);
  case 118: return loadTrueDevilKazuya(moveset, bossCode);
  case 121: return loadStoryDevilJin(moveset, bossCode);
  default: return false;
  }
}

void disableStoryRelatedReqs(uintptr_t requirements, int givenReq = 228)
{
  for (uintptr_t addr = requirements; true; addr += Sizes::Requirement)
  {
    int req = Game.readUInt32(addr);
    if (req == END_REQ)
      break;
    if ((std::find(STORY_REQS.begin(), STORY_REQS.end(), req) != STORY_REQS.end()) || req == givenReq)
    {
      Game.write<uintptr_t>(addr, 0);
    }
  }
}

bool loadJin(uintptr_t moveset, int bossCode)
{
  int _777param = bossCode == BossCodes::ChainedJin ? 1 : bossCode;
  // Disabling requirements
  disableRequirements(moveset, STORY_FLAGS_REQ, _777param);

  // Adjusting Rage Art
  uintptr_t rageArt = getMoveAddress(moveset, 0x9BAE061E, 2100);
  if (rageArt && bossCode != 0)
  {
    uintptr_t cancel = Game.readUInt64(rageArt + Offsets::Move::CancelList);
    int regularRA = getMoveId(moveset, 0x1ADAB0CB, 2000);
    if (regularRA != -1)
    {
      Game.write<short>(cancel + Offsets::Cancel::Move, regularRA);
    }
  }

  switch (bossCode)
  {
  case BossCodes::RegularJin:
  {
    uintptr_t addr = getMoveAddress(moveset, 0x9b789d36, 1865); // d/b+1+2
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 0));
  }
  break;
  case BossCodes::NerfedJin:
  {
    // disableRequirements(moveset, 228, 1);
    // disableRequirements(moveset, STORY_BATTLE_REQ - 1, 0);
    // disableRequirements(moveset, STORY_BATTLE_REQ, 33);
  }
  break;
  case BossCodes::MishimaJin:
  case BossCodes::KazamaJin:
  {
    uintptr_t moveId = getMoveId(moveset, bossCode == 2 ? 0xA33CD19D : 0x7614EF15, 2000);
    if (moveId != 0)
    {
      Game.write<short>(moveset + 0xAA, moveId);
    }
  }
  break;
  case BossCodes::ChainedJin:
  {
    uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);

    std::vector<std::pair<int, int>> moves = {
        {0xCAD0CF3C, 1500}, // 1+2
        {0xE383D012, 2000}, // f,f+2
        {0xEEE71DFB, 1400}, // b+1+2
        {0x9B789D36, 1600}  // d/b+1+2
    };

    for (const auto &move : moves)
    {
      uintptr_t moveAddr = getMoveAddress(moveset, move.first, move.second);
      if (moveAddr)
      {
        Game.write<uintptr_t>(getMoveNthCancel(moveAddr, 0) + Offsets::Cancel::RequirementsList, reqHeader);
      }
    }
  }
  break;
  default:
    return false;
  }

  return markMovesetEdited(moveset);
}

bool loadKazuya(uintptr_t moveset, int bossCode)
{
  int defaultAliasIdx = Game.readUInt16(moveset + 0x30);
  int idleStanceIdx = Game.readUInt16(moveset + 0x32);

  if (bossCode == BossCodes::DevilKazuya)
  {
    // Enabling permanent Devil form
    uintptr_t movesHeader = Game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
    uintptr_t addr = movesHeader + (idleStanceIdx * Sizes::Moveset::Move);
    addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList);
    Game.write<int>(addr + Offsets::ExtraProp::Prop, 0x8151);
    Game.write<int>(addr + Offsets::ExtraProp::Value, 1);

    // Disabling some requirements for basic attacks
    // 0x8000 alias
    // addr = movesHeader + (defaultAliasIdx * Sizes::Moveset::Move);
    // 32th cancel
    // disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 31), 777);

    addr = getMoveAddress(moveset, 0x42CCE45A, idleStanceIdx); // CD+4, 1 last hit key
    // Iteratively find the first cancel which has the req 667
    {
      uintptr_t cancel = getMoveNthCancel(addr, 0);
      for (; true; cancel += Sizes::Moveset::Cancel)
      {
        if (cancelHasCondition(cancel, STORY_BATTLE_REQ - 1, -1))
        {
          break;
        }
      }
      disableStoryRelatedReqs(getCancelReqAddr(cancel));
      // Move 2 cancels forward
      cancel += Sizes::Moveset::Cancel * 2;
      disableStoryRelatedReqs(getCancelReqAddr(cancel));
    }

    // 1,1,2
    addr = getMoveAddress(moveset, 0x2226A9EE, idleStanceIdx);
    // 3rd cancel
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 2));

    // Juggle Escape
    addr = getMoveAddress(moveset, 0xDEBED999, 5);
    // 6th cancel
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 6));
    // 7th cancel
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 7));

    // f,f+2
    addr = getMoveAddress(moveset, 0x1A571FA1, 2000);
    // 20th cancel
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 21));
    // 21st cancel
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 22));

    // d/b+1+2
    addr = getMoveAddress(moveset, 0x73EBDBA2, idleStanceIdx);
    addr = getMoveNthCancel(addr, 0); // 1st cancel
    // 3rd cancel
    {
      uintptr_t req0 = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
      Game.write<uintptr_t>((addr + Sizes::Moveset::Cancel * 2) + Offsets::Cancel::RequirementsList, req0);
    }

    // d/b+4
    addr = getMoveAddress(moveset, 0x9364E2F5, idleStanceIdx);
    // 1st cancel
    addr = getMoveNthCancel1stReqAddr(addr, 0);
    disableStoryRelatedReqs(addr);
    // Disabling standing req
    Game.write<int>(addr + Sizes::Moveset::Requirement, 0);

    return markMovesetEdited(moveset);
  }
  else if (bossCode == BossCodes::FinalKazuya)
  {
    // Go through reqs and props to disable his devil form
    // requirements
    uintptr_t start = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
    uintptr_t count = Game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
    for (uintptr_t i = 4530; i < count - 2000; i++) // I know around req 4530 these requirements first appear
    { //4534, 4660 // 4794, 4803 (indexes for these props that I found in v1.09)
      uintptr_t addr = start + (i * Sizes::Moveset::Requirement);
      int req = Game.readUInt32(addr);
      int param = Game.readUInt32(addr + 4);
      if ((req == 0x80dc && param >= 1) || (req == 0x8683))
      {
        Game.write<int>(addr, 0);
        Game.write<int>(addr + 4, 0);
      }
    }

    // extraprops
    start = Game.readUInt64(moveset + Offsets::Moveset::ExtraMovePropertiesHeader);
    count = Game.readUInt64(moveset + Offsets::Moveset::ExtraMovePropertiesCount);
    for (uintptr_t i = 2200; i < count; i++) // First time they appear is at around idx 2275
    {
      uintptr_t addr = start + (i * Sizes::Moveset::ExtraMoveProperty);
      int prop = Game.readUInt32(addr + Offsets::ExtraProp::Prop);
      int param = Game.readUInt32(addr + Offsets::ExtraProp::Value);
      if (prop == 0x80dc || prop == 0x8683 || (prop == 0x8039 && (param == 0xC || param == 0xD)))
      {
        Game.write<int>(addr, 0);
        Game.write<int>(addr + 4, 0);
      }
    }

    // Single-spin uppercut
    uintptr_t addr = getMoveAddress(moveset, 0xD172C176, idleStanceIdx); // B+1+4
    // 2nd cancel
    addr = getMoveNthCancel(addr, 1);
    Game.write<int>(addr, 0x10);
    Game.write<short>(addr + Offsets::Cancel::Option, 0x50);

    uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
    // Ultra-wavedash
    addr = getMoveAddress(moveset, 0x77314B09, idleStanceIdx); // 2
    // Replacing 2nd cancel requirement list index
    addr = getMoveNthCancel(addr, 1);
    Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);

    // CD+1+2
    addr = getMoveAddress(moveset, 0x0C9CE140, idleStanceIdx);
    // Storing the story cancel req
    uintptr_t storyReq = getMoveNthCancel1stReqAddr(addr, 0);
    // Replacing 1st cancel requirement list index
    Game.write<uintptr_t>(getMoveNthCancel(addr, 0) + Offsets::Cancel::RequirementsList, reqHeader);

    // ff2
    // addr = getMoveAddress(moveset, 0xC00BB85A, idleStanceIdx); // 2
    // 21st cancel
    // addr = Game.readUInt64(addr + TekkenOffsets::Move::CancelList) + (40 * 20);
    // Game.write<uintptr_t>(addr + TekkenOffsets::Cancel::RequirementsList, reqHeader);

    // b+2,2
    addr = getMoveAddress(moveset, 0x8B5BFA6C, idleStanceIdx); // 2nd hit of b+2,2
    // Replacing 1st cancel requirement list index
    Game.write<uintptr_t>(getMoveNthCancel(addr, 0) + Offsets::Cancel::RequirementsList, reqHeader);

    // NEW b+2,2. Disabling laser cancel
    addr = getMoveAddress(moveset, 0x8FE28C6A, defaultAliasIdx); // 2nd hit of b+2,2
    {
      addr = getMoveNthCancel(addr, 1); // 2nd cancel
      uintptr_t cancelExtradata = getNthCancelFlagAddr(moveset, 60);
      Game.write<int>(addr + Offsets::Cancel::CancelExtradata, cancelExtradata);
    }

    // Disabling u/b+1+2 laser
    // key ub1: 0x1376C644
    // key ub1+2: 0x07F32E0C
    addr = getMoveAddress(moveset, 0x07F32E0C, 2000); // u/b+1+2
    addr = getMoveNthCancel(addr, 0);
    {
      uintptr_t cancelExtradata = getNthCancelFlagAddr(moveset, 16);
      int moveId = getMoveId(moveset, 0x1376C644, idleStanceIdx);
      // Making a cancel to u/b+1 on frame 1
      Game.write<int>(addr + Offsets::Cancel::RequirementsList, reqHeader);
      Game.write<int>(addr + Offsets::Cancel::CancelExtradata, cancelExtradata);
      Game.write<int>(addr + Offsets::Cancel::TransitionFrame, 1);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId);
      Game.write<short>(addr + Offsets::Cancel::Option, 65);
    }

    // d/f+3+4, 1 key: 0x6562FA84
    addr = getMoveAddress(moveset, 0x6562FA84, idleStanceIdx); // (d/f+3+4),1
    addr = getMoveNthCancel(addr, 0);
    {
      int moveId = Game.readInt16((addr + Sizes::Moveset::Cancel) + Offsets::Cancel::Move);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId);
    }

    // d/b+1, 2
    addr = getMoveAddress(moveset, 0xFE501006, idleStanceIdx); // d/b+1
    addr = getMoveNthCancel(addr, 0);
    {
      // Grabbing move ID from 3rd cancel
      int moveId_db2 = Game.readInt16(addr + (2 * Sizes::Moveset::Cancel) + Offsets::Cancel::Move);
      // 10th cancel
      addr = addr + (9 * Sizes::Moveset::Cancel);
      Game.write<int>(addr + Offsets::Cancel::WindowStart, 19);
      Game.write<int>(addr + Offsets::Cancel::WindowEnd, 19);
      Game.write<int>(addr + Offsets::Cancel::TransitionFrame, 19);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId_db2);
      // 11th cancel
      addr += Sizes::Moveset::Cancel;
      Game.write<int>(addr + Offsets::Cancel::WindowStart, 19);
      Game.write<int>(addr + Offsets::Cancel::WindowEnd, 19);
      Game.write<int>(addr + Offsets::Cancel::TransitionFrame, 19);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId_db2);

      // Adjusting d/b+1+2 to cancel into this on frame-1
      int moveId_db1 = getMoveId(moveset, 0xFE501006, moveId_db2);
      uintptr_t cancelExtradata = getNthCancelFlagAddr(moveset, 20);
      addr = getMoveAddress(moveset, 0x73EBDBA2, moveId_db1);
      addr = getMoveNthCancel(addr, 0);
      // Adjusting the 1st cancel
      Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId_db1);
    }

    // ws+2
    addr = getMoveAddress(moveset, 0xB253E5F2, idleStanceIdx);                         // d/b+1
    addr = getMoveNthCancel(addr, 1); // 2nd cancel
    {
      uintptr_t cancelExtradata = getNthCancelFlagAddr(moveset, 20);
      int moveId = getMoveId(moveset, 0x0AB42E52, defaultAliasIdx);
      Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);
      Game.write<uintptr_t>(addr + Offsets::Cancel::CancelExtradata, cancelExtradata);
      Game.write<int>(addr + Offsets::Cancel::WindowStart, 5);
      Game.write<int>(addr + Offsets::Cancel::WindowEnd, 5);
      Game.write<int>(addr + Offsets::Cancel::TransitionFrame, 5);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId);
      Game.write<short>(addr + Offsets::Cancel::Option, 65);
    }

    markMovesetEdited(moveset);
  }
  return false;
}

bool loadAzazel(uintptr_t moveset, int bossCode)
{
  if (bossCode != BossCodes::Azazel) return false;
  int defaultAliasIdx = Game.readUInt16(moveset + 0x30);
  uintptr_t addr = getMoveAddressByIdx(moveset, defaultAliasIdx);
  addr = Game.readUInt64(addr + Offsets::Move::CancelList);         // cancel
  addr = Game.readUInt64(addr + Offsets::Cancel::RequirementsList); // 1st req
  Game.write<int>(addr + 4, 8);
  addr += Sizes::Moveset::Requirement; // 2nd req
  Game.write<int>(addr, 675);
  Game.write<int>(addr + 4, 0);
  addr += Sizes::Moveset::Requirement; // 3rd req
  Game.write<int>(addr, 679);
  Game.write<int>(addr + 4, 0);

  return markMovesetEdited(moveset);
}

bool isCorrectHeihachiFlag(int storyFlag, int param)
{
  switch (storyFlag)
  {
  case 1:
    return (param >= 0x501 && param < 0x601);
  case 2:
    return (param >= 0x601 && param < 0x701);
  case 3:
    return (param >= 0x801);
  default:
    break;
  }
  return false;
}

void handleHeihachiMoveProp(uintptr_t moveset, int moveIdx)
{
  uintptr_t addr = getMoveAddressByIdx(moveset, moveIdx);
  addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList);
  while (true)
  {
    int frame = Game.readInt32(addr + Offsets::ExtraProp::Type);
    int prop = Game.readInt32(addr + Offsets::ExtraProp::Prop);
    uintptr_t reqList = Game.readInt32(addr + Offsets::ExtraProp::RequirementAddr);
    if (!prop && !frame)
      break;
    if (prop == 0x82e2)
    {
      Game.write<int>(addr + Offsets::ExtraProp::Value, 0); // don't spend rage
    }
    // Cancels & Props both have requirements at offset 0x8
    if (cancelHasCondition(addr, 802, 2050))
    {
      disableStoryRelatedReqs(getCancelReqAddr(addr));
      break;
    }
    addr += Sizes::Moveset::ExtraMoveProperty;
  }
}

bool loadHeihachi(uintptr_t moveset, int bossCode)
{
  if (bossCode / 10 != 35) return false;
  int defaultAliasIdx = Game.readUInt16(moveset + 0x30);
  int idleStanceIdx = Game.readUInt16(moveset + 0x32);
  uintptr_t addr = getMoveAddressByIdx(moveset, idleStanceIdx);

  // Idle stance, set/disable Warrior Instinct
  addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList); // props
  addr = addr + 4 * Sizes::Moveset::ExtraMoveProperty;         // 5th prop
  Game.write<int>(addr + Offsets::ExtraProp::Prop, 0x83F9);
  Game.write<int>(addr + Offsets::ExtraProp::Value, (int)(bossCode == BossCodes::FinalHeihachi));

  if (bossCode == BossCodes::ShadowHeihachi)
  {
    addr = getMoveAddressByIdx(moveset, idleStanceIdx);
    uintptr_t cancel1 = getMoveNthCancel(addr, 0);
    uintptr_t reqListCancel1 = getCancelReqAddr(cancel1);
    uintptr_t reqListCancel2 = getMoveNthCancel1stReqAddr(addr, 1);
    Game.write<uintptr_t>(cancel1 + Offsets::Cancel::RequirementsList, reqListCancel2);
    disableStoryRelatedReqs(reqListCancel1);
    // TODO: b,f+2 functional
    // TODO: Broken Toy functional
    return markMovesetEdited(moveset);
  }

  // Final Heihachi
  if (bossCode == BossCodes::FinalHeihachi)
  {
    // Enable most of the moves by modifying 2,2
    addr = getMoveAddress(moveset, 0xF69E2BEF, 1550);
    addr = getMoveNthCancel(addr, 1);
    disableStoryRelatedReqs(getCancelReqAddr(addr));
    int new22 = Game.readInt16(addr + Offsets::Cancel::Move);
    addr = getMoveAddressByIdx(moveset, new22);
    // 2,2,2
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 5));
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 6));
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 7));
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 8));
    // 1,1 > 1+3 throw
    addr = getMoveAddress(moveset, 0x10E04C8A, 2000);
    disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 0));
    // Parry cancels from idle stance
    addr = getMoveAddressByIdx(moveset, idleStanceIdx);
    addr = getMoveNthCancel(addr, 0);
    // Finding the address for the first parry cancel
    for (uintptr_t cancel = addr; true; cancel += Sizes::Moveset::Cancel)
    {
      if (cancelHasCondition(cancel, 806, 3))
      {
        addr = cancel;
        break;
      }
    }
    // 4-cancels for parries
    for (int i = 0; i < 4; i++)
    {
      disableStoryRelatedReqs(getCancelReqAddr(addr));
      addr += Sizes::Moveset::Cancel;
    }

    int preRound1 = defaultAliasIdx - 3;
    int preRound2 = defaultAliasIdx - 2;
    {
      uintptr_t defaultAliasAddr = getMoveAddressByIdx(moveset, defaultAliasIdx);
      addr = getMoveNthCancel(defaultAliasAddr, 50);
      while (true)
      {
        if (cancelHasCondition(addr, 696, -1))
          break;
        addr += Sizes::Moveset::Cancel;
      }
      // 
      Game.write<uint16_t>(addr + Offsets::Cancel::Move, preRound1);
      addr += Sizes::Moveset::Cancel; // going to next cancel
      Game.write<uint16_t>(addr + Offsets::Cancel::Move, preRound2);
      // Now enabling story reqs inside their props
      handleHeihachiMoveProp(moveset, preRound1);
      handleHeihachiMoveProp(moveset, preRound2);
    }
    return markMovesetEdited(moveset);
  }

  uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
  uintptr_t reqCount = Game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
  int req = 0, param = 0;
  int storyFlag = bossCode - 350;
  for (uintptr_t i = 0; i < reqCount; i++)
  {
    addr = reqHeader + i * Sizes::Moveset::Requirement;
    req = Game.readInt32(addr);
    param = Game.readInt32(addr + 4);
    if ((req == 806 && param == storyFlag) || req == 801 || (req == 802 && isCorrectHeihachiFlag(storyFlag, param)))
    {
      Game.write<int64_t>(addr, 0);
    }
  }

  return markMovesetEdited(moveset);
}

bool loadAngelJin(uintptr_t moveset, int bossCode)
{
  if (bossCode != BossCodes::AngelJin) return false;
  adjustIntroOutroReq(moveset, bossCode, 2085); // I know targetReq is first seen after index 2085

  return markMovesetEdited(moveset);
}

bool loadTrueDevilKazuya(uintptr_t moveset, int bossCode)
{
  if (bossCode != BossCodes::TrueDevilKazuya) return false;
  adjustIntroOutroReq(moveset, bossCode, 2900); // I know targetReq is first seen after index 2900
  // d/f+1, 2
  uintptr_t addr = getMoveAddress(moveset, 0x4339a4bd, 1673);
  disableStoryRelatedReqs(getMoveNthCancel1stReqAddr(addr, 22), 473); // 23rd cancel

  return markMovesetEdited(moveset);
}

bool loadStoryDevilJin(uintptr_t moveset, int bossCode)
{
  if (bossCode != BossCodes::DevilJin) return false;
  int defaultAliasIdx = Game.readUInt16(moveset + 0x30);
  uintptr_t addr = 0;
  adjustIntroOutroReq(moveset, bossCode, 2000); // I know targetReq is first seen after index 2000

  // Adjusting winposes
  {
    int enderId = getMoveId(moveset, 0xAB7FA036, defaultAliasIdx); // Grabbed ID of the match-ender
    // Grabbing ID of the first intro from alias 0x8000
    addr = getMoveAddressByIdx(moveset, defaultAliasIdx);
    addr = getMoveNthCancel(addr, 1); // 2nd Cancel
    int start = Game.readUInt16(addr + Offsets::Cancel::Move);

    uintptr_t cancel = 0;
    addr = getMoveAddress(moveset, 0xD9CDC1C0, start);
    for (int i = 0; i < 3; i++)
    {
      cancel = getMoveNthCancel(addr, 0);
      Game.write<int16_t>(cancel + Offsets::Cancel::Move, enderId);
      addr += Sizes::Moveset::Move;
    }
  }
  // Rage Art init (0xa02e070b)
  addr = getMoveAddress(moveset, 0xa02e070b, defaultAliasIdx - 20);
  addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList);
  disableStoryRelatedReqs(Game.readUInt64(addr + Offsets::ExtraProp::RequirementAddr));
  // Rage Art throw (0xfe2cd621)
  addr = getMoveAddress(moveset, 0xfe2cd621, defaultAliasIdx - 15);
  // 1st extraprop
  addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList);
  disableStoryRelatedReqs(Game.readUInt64(addr + Offsets::ExtraProp::RequirementAddr));
  // 5th extraprop
  addr += 4 * Sizes::Moveset::ExtraMoveProperty;
  disableStoryRelatedReqs(Game.readUInt64(addr + Offsets::ExtraProp::RequirementAddr));

  return markMovesetEdited(moveset);
}

uintptr_t getMoveAddress(uintptr_t moveset, int moveNameKey, int start = 0)
{
  uintptr_t movesHead = Game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
  int movesCount = Game.readInt32(moveset + Offsets::Moveset::MovesCount);
  start = start >= movesCount ? 0 : start;
  int rawIdx = -1;
  for (int i = start; i < movesCount; i++)
  {
    rawIdx = (i % 8) - 4;
    uintptr_t addr = movesHead + i * Sizes::Moveset::Move;
    if (rawIdx > -1)
    {
      int value = Game.readInt32(addr + 0x10 + rawIdx * 4);
      if (value == moveNameKey)
        return addr;
    }
    else
    {
      EncryptedValue *paramAddr = reinterpret_cast<EncryptedValue *>(addr);
      uintptr_t decryptedValue = Game.callFunction<uintptr_t, EncryptedValue>(DECRYPT_FUNC_ADDR, paramAddr);
      if ((int)decryptedValue == moveNameKey)
        return addr;
    }
  }
  std::ostringstream oss;
  oss << "Failed to find the desired address: moveNameKey=0x" << std::hex << moveNameKey;
  throw std::runtime_error(oss.str());
}

uintptr_t getMoveAddressByIdx(uintptr_t moveset, int idx)
{
  uintptr_t movesHead = Game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
  int movesCount = Game.readInt32(moveset + Offsets::Moveset::MovesCount);
  idx = idx >= movesCount ? idx % movesCount : idx;
  return movesHead + idx * Sizes::Moveset::Move;
}

int getMoveId(uintptr_t moveset, int moveNameKey, int start = 0)
{
  uintptr_t movesHead = Game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
  int movesCount = Game.readUInt64(moveset + Offsets::Moveset::MovesCount);
  start = start >= movesCount ? 0 : start;
  uintptr_t addr = 0;
  int rawIdx = -1;
  for (int i = start; i < movesCount; i++)
  {
    rawIdx = (i % 8) - 4;
    addr = movesHead + i * Sizes::Moveset::Move;
    if (rawIdx > -1)
    {
      int value = Game.readInt32(addr + 0x10 + rawIdx * 4);
      if (value == moveNameKey)
        return i;
    }
    else
    {
      EncryptedValue *paramAddr = reinterpret_cast<EncryptedValue *>(addr);
      uintptr_t decryptedValue = Game.callFunction<uintptr_t, EncryptedValue>(DECRYPT_FUNC_ADDR, paramAddr);
      if ((int)decryptedValue == moveNameKey)
        return i;
    }
  }
  std::ostringstream oss;
  oss << "Failed to find the desired address: moveNameKey=0x" << std::hex << moveNameKey;
  throw std::runtime_error(oss.str());
  return -1;
}

bool funcAddrIsValid(uintptr_t funcAddr)
{
  byte originalBytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x59, 0x08, 0x48, 0x8B};
  std::vector<byte> gameBytes = Game.readArray<byte>(funcAddr, 16);

  if (gameBytes.size() != sizeof(originalBytes))
  {
    return false;
  }

  for (size_t i = 0; i < sizeof(originalBytes); ++i)
  {
    if (gameBytes[i] != originalBytes[i])
    {
      return false;
    }
  }

  return true;
}

bool movesetExists(uintptr_t moveset)
{
  std::string str = Game.ReadString(moveset + 8, 3);
  return str.compare("ALI") == 0 || str.compare("TEK") == 0;
}

bool isMovesetEdited(uintptr_t moveset)
{
  std::string str = Game.ReadString(moveset + 8, 3);
  return str.compare("ALI") == 0;
}

// Checks if it's eligible to load the boss character
bool isEligible(uintptr_t matchStructAddr)
{
  int value = Game.readInt32(matchStructAddr);
  return value == 1 || value == 2 || value == 5 || value == 6 || value == 12;
}

bool cancelHasCondition(uintptr_t cancel, int targetReq, int targetParam = -1)
{
  uintptr_t requirements = getCancelReqAddr(cancel);
  for (uintptr_t addr = requirements; true; addr += Sizes::Moveset::Requirement)
  {
    int req = Game.readInt32(addr);
    int param = Game.readInt32(addr + 4);
    if (req == END_REQ)
      return false;
    if (req == targetReq && (targetParam == -1 || param == targetParam))
    {
      return true;
    }
  }
  return false;
}

void adjustIntroOutroReq(uintptr_t moveset, int bossCode, int start = 0)
{
  uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
  uintptr_t reqCount = Game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
  uintptr_t requirement = 0;
  int req = -1;
  for (int i = start; i < reqCount; i++)
  {
    requirement = reqHeader + i * Sizes::Moveset::Requirement;
    req = Game.readInt32(requirement);
    if (req == 755)
    {
      Game.write(requirement + 4, bossCode);
    }
  }
}

uintptr_t getCancelReqAddr(uintptr_t cancel)
{
  return Game.readUInt64(cancel + Offsets::Cancel::RequirementsList);
}

uintptr_t getMoveNthCancel(uintptr_t move, int n)
{
  return Game.readUInt64(move + Offsets::Move::CancelList) + Sizes::Moveset::Cancel * n;
}

uintptr_t getMoveNthCancel1stReqAddr(uintptr_t move, int n)
{
  return getCancelReqAddr(getMoveNthCancel(move, n));
}

uintptr_t getNthCancelFlagAddr(uintptr_t moveset, int n)
{
  return Game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasHeader) + Sizes::Moveset::CancelExtradata * n;
}

bool markMovesetEdited(uintptr_t moveset)
{
  try
  {
    Game.writeString(moveset + 8, "ALI");
    return true;
  }
  catch (...)
  {
    return false;
  }
}

void modifyHudAddr(uintptr_t matchStructAddr)
{
  int mode = Game.readInt32(matchStructAddr);
  if (mode == 1 || mode == 6)
  {
    int icon = Game.readUInt16(HUD_ICON_ADDR);
    int name = Game.readUInt16(HUD_NAME_ADDR);
    if (icon == 0x5274 && name == 0x3174)
    {
      Game.write<uint16_t>(HUD_ICON_ADDR, 0x9090);
      Game.write<uint16_t>(HUD_NAME_ADDR, 0x9090);
    }
  }
}

void restoreHudAddr(uintptr_t matchStructAddr)
{
  int icon = Game.readUInt16(HUD_ICON_ADDR);
  int name = Game.readUInt16(HUD_NAME_ADDR);
  if (icon == 0x9090 && name == 0x9090)
  {
    Game.write<uint16_t>(HUD_ICON_ADDR, 0x5274);
    Game.write<uint16_t>(HUD_NAME_ADDR, 0x3174);
  }
}

BOOL WINAPI ConsoleHandler(DWORD signal)
{
  if (signal == CTRL_CLOSE_EVENT)
  {
    restoreHudAddr(0);
    return TRUE;
  }
  return FALSE;
}

void signalHandler(int signal = 1)
{
	restoreHudAddr(0);
	std::exit(signal);
}

void atExit()
{
	restoreHudAddr(0);
}
