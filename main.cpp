#include "Game.h"
#include "utils.h"
#include "tekken.h"
#include <conio.h>
#include <unistd.h>
#include <limits>

using namespace Tekken;

// Globals
GameClass Game;
std::map<std::string, uintptr_t> addresses;
uintptr_t MOVESET_OFFSET = 0;
uintptr_t DECRYPT_FUNC_ADDR = 0;
uintptr_t PERMA_DEVIL_OFFSET = 0;
uintptr_t PLAYER_STRUCT_BASE = 0;
uintptr_t MATCH_STRUCT_OFFSET = 0;

std::string FINAL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_naked.CS_ant_1p_naked";
std::string CHAINED_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_ant_1p_chain.CS_ant_1p_chain";
std::string FINAL_KAZ_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_grl_1p_v2_white.CS_grl_1p_v2_white";
std::string DEVIL_JIN_COSTUME_PATH = "/Game/Demo/Story/Sets/CS_swl_ant_1p.CS_swl_ant_1p";

bool IS_WRITTEN = false;
int STORY_FLAGS_REQ = 777;
int STORY_BATTLE_REQ = 668;
int SIDE_SELECTED = 0;

struct EncryptedValue
{
  uintptr_t value;
  uintptr_t key;
};

void storeAddresses();
int getSideSelection();
void mainFunc(int bossCode);
int takeInput();
void loadCostume(uintptr_t matchStructAddr, int costumeId, std::string costumePath);
void costumeHandler(uintptr_t matchStructAddr, int bossCode);
void sleep(int ms) { usleep(ms * 1000); }
bool loadBoss(uintptr_t playerAddr, uintptr_t moveset, int bossCode);
void loadCharacter(uintptr_t matchStructAddr, int bossCode);
bool loadJin(uintptr_t moveset, int bossCode);
bool loadKazuya(uintptr_t moveset, int bossCode);
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

int main()
{
  int bossCode = -1;
  if (Game.Attach(L"Polaris-Win64-Shipping.exe"))
  {
    printf("Attached to the Game\n");
    SIDE_SELECTED = getSideSelection();
    addresses = readKeyValuePairs("addresses.txt");
    storeAddresses();
    // Validating the function address
    if (!funcAddrIsValid(DECRYPT_FUNC_ADDR))
    {
      printf("Function address is invalid. The script will not be able to work if this is not correct.\nPress any key to close the script\n");
      _getch();
      return 0;
    }
    bossCode = takeInput();
    if (bossCode != -1)
      mainFunc(bossCode);
  }
  if (bossCode != -1)
  {
    printf("Press any key to close the script\n");
    _getch();
  }
  return 0;
}

void storeAddresses()
{
  PERMA_DEVIL_OFFSET = getValueByKey(addresses, "permanent_devil_offset");
  PLAYER_STRUCT_BASE = getValueByKey(addresses, "player_struct_base");
  MOVESET_OFFSET = getValueByKey(addresses, "moveset_offset");
  DECRYPT_FUNC_ADDR = getValueByKey(addresses, "decryption_function_offset") + Game.getBaseAddress();
  MATCH_STRUCT_OFFSET = getValueByKey(addresses, "match_struct");
}

int getSideSelection()
{
  int selectedSide;
  while (true)
  {
    std::cout << "\nWhich side do you want to activate this script for? Enter 0 for Left side and 1 for Right side: ";
    std::cin >> selectedSide;
    if (std::cin.fail() || (selectedSide != 0 && selectedSide != 1))
    {
      std::cin.clear();                                                   // clear the error flag
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // discard invalid input
      std::cout << "Invalid input! Please enter 0 or 1." << std::endl;
    }
    else
    {
      break;
    }
  }
  return selectedSide;
}

int takeInput()
{
  printf("Select the Jin that you want to play as\n");
  printf("Please note, this script only works for offline mods and for player 1\n");
  printf("1. Devil-powered Jin from Chapter 1\n");
  printf("2. Nerfed Jin\n");
  printf("3. Chained Jin from Chapter 12 Battle 3\n");
  printf("4. Mishima Jin from Chapter 15 Battle 2\n");
  printf("5. Kazama Jin from Chapter 15 Battle 3\n");
  printf("6. Awakened Jin from Chapter 15 Final Battle\n");
  printf("7. Devil Kazuya from Chapter 6\n");
  printf("8. Final Battle Kazuya\n");
  printf("9. Heihachi from Story DLC Finale\n");
  printf("A. Angel Jin\n");
  printf("B. True Devil Kazuya\n");
  printf("C. Story Devil Jin\n");
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
    return BossCodes::FinalHeihachi;
  case 'A':
  case 'a':
    return BossCodes::AngelJin;
  case 'B':
  case 'b':
    return BossCodes::TrueDevilKazuya;
  case 'C':
  case 'c':
    return BossCodes::DevilJin;
  default:
    return -1;
  }
  return -1;
}

void mainFunc(int bossCode)
{
  // system("cls");
  printf("Please load into your Game mode now, the Game will automatically detect & load the altered moveset\n");
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

    if (Game.readInt32(matchStructAddr) != 1 && Game.readInt32(matchStructAddr) != 6)
    {
      continue;
    }

    // Set Character ID
    loadCharacter(matchStructAddr, bossCode);

    uintptr_t playerAddr = Game.getAddress({(DWORD)PLAYER_STRUCT_BASE, (DWORD)(0x30 + SIDE_SELECTED * 8)});
    if (playerAddr == 0)
    {
      costumeHandler(matchStructAddr, bossCode);
      continue;
    }

    uintptr_t movesetAddr = Game.ReadUnsignedLong(playerAddr + MOVESET_OFFSET);
    if (movesetAddr == 0)
    {
      isWritten = false;
      continue;
    }

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
        printf("Moveset Edited\n");
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
  switch (bossCode)
  {
  case BossCodes::RegularJin:
  case BossCodes::NerfedJin:
  case BossCodes::DevilKazuya:
  case BossCodes::FinalHeihachi:
    loadCostume(matchStructAddr, 0, "\0");
    break;
  case BossCodes::ChainedJin:
    loadCostume(matchStructAddr, 51, CHAINED_JIN_COSTUME_PATH);
    break;
  case BossCodes::MishimaJin:
  case BossCodes::KazamaJin:
  case BossCodes::FinalJin:
    loadCostume(matchStructAddr, 51, FINAL_JIN_COSTUME_PATH);
    break;
  case BossCodes::FinalKazuya:
    loadCostume(matchStructAddr, 51, FINAL_KAZ_COSTUME_PATH);
    break;
  case BossCodes::DevilJin:
    loadCostume(matchStructAddr, 51, DEVIL_JIN_COSTUME_PATH);
    break;
  default:
    break;
  }
}

void loadCharacter(uintptr_t matchStructAddr, int bossCode)
{
  int charId = -1;
  int currCharId = Game.readInt32(matchStructAddr + 0x10 + SIDE_SELECTED * 0x84);
  switch (bossCode)
  {
  case BossCodes::AngelJin:
    charId = BossCodes::AngelJin;
    // charId = currCharId == 6 ? BossCodes::AngelJin : -1;
    break;
  case BossCodes::TrueDevilKazuya:
    charId = BossCodes::TrueDevilKazuya;
    // charId = currCharId == 8 ? BossCodes::TrueDevilKazuya : -1;
    break;
  case BossCodes::DevilJin:
    charId = BossCodes::DevilJin;
    // charId = currCharId == 12 ? BossCodes::DevilJin : -1;
    break;
  default:
    return;
  }
  if (charId != -1) {
    Game.write<int>(matchStructAddr + 0x10 + SIDE_SELECTED * 0x84, charId);
    if (charId == BossCodes::DevilJin) loadCostume(matchStructAddr, 51, DEVIL_JIN_COSTUME_PATH); // Just a safety precaution
    // for (int i = 0; i < 100; i++) {
    //   sleep(10);
    //   Game.write<int>(matchStructAddr + 0x10 + SIDE_SELECTED * 0x84, charId);
    // }
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
  if (charId == 6)
  {
    return loadJin(moveset, bossCode);
  }
  else if (charId == 8)
  {
    if (bossCode == 97)
    {
      Game.write<int>(playerAddr + PERMA_DEVIL_OFFSET, 1);
    }
    return loadKazuya(moveset, bossCode);
  }
  else if (charId == 35)
  {
    return loadHeihachi(moveset, bossCode);
  }
  else if (charId == 118)
  {
    return loadTrueDevilKazuya(moveset, bossCode);
  }
  return true;
}

void disableStoryRelatedReqs(uintptr_t requirements, int givenReq = 228)
{
  for (uintptr_t addr = requirements; true; addr += Sizes::Requirement)
  {
    int req = Game.readUInt32(addr);
    if (req == 1100)
      break;
    if (req == STORY_BATTLE_REQ || req == STORY_BATTLE_REQ - 1 || req == givenReq)
    {
      Game.write<uintptr_t>(addr, 0);
    }
  }
}

bool loadJin(uintptr_t moveset, int bossCode)
{
  int _777param = bossCode == 11 ? 1 : bossCode;
  // Disabling requirements
  disableRequirements(moveset, STORY_FLAGS_REQ, _777param);

  // Adjusting Rage Art
  uintptr_t rageArt = getMoveAddress(moveset, 0x9BAE061E, 2100);
  if (rageArt)
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
  case 0:
  {
    disableRequirements(moveset, STORY_BATTLE_REQ, 17);
  }
  break;
  case 1:
  {
    // disableRequirements(moveset, 228, 1);
    // disableRequirements(moveset, STORY_BATTLE_REQ - 1, 0);
    // disableRequirements(moveset, STORY_BATTLE_REQ, 33);
  }
  break;
  case 2:
  case 3:
  {
    uintptr_t moveId = getMoveId(moveset, bossCode == 2 ? 0xA33CD19D : 0x7614EF15, 2000);
    if (moveId != 0)
    {
      Game.write<short>(moveset + 0xAA, moveId);
    }
  }
  break;
  case 11:
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
        uintptr_t cancel = Game.readUInt64(moveAddr + Offsets::Move::CancelList);
        Game.write<uintptr_t>(cancel + Offsets::Cancel::RequirementsList, reqHeader);
      }
    }
  }
  break;
  default:
    break;
  }

  Game.writeString(moveset + 8, "ALI");
  return true;
}

bool loadKazuya(uintptr_t moveset, int bossCode)
{
  int defaultAliasIdx = Game.readUInt16(moveset + 0x30);
  int idleStanceIdx = Game.readUInt16(moveset + 0x32);

  // Chapter 6 Devil Kazuya
  if (bossCode == 97)
  {
    // Enabling permanent Devil form
    uintptr_t movesHeader = Game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
    uintptr_t addr = movesHeader + (idleStanceIdx * Sizes::Moveset::Move);
    addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList);
    Game.write<int>(addr + Offsets::ExtraProp::Prop, 0x8151);
    Game.write<int>(addr + Offsets::ExtraProp::Value, 1);

    // Disabling some requirements for basic attacks
    // 0x8000 alias
    addr = movesHeader + (defaultAliasIdx * Sizes::Moveset::Move);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList); // cancel list
    // 32th cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 31) + Offsets::Cancel::RequirementsList), 777);

    addr = getMoveAddress(moveset, 0x42CCE45A, idleStanceIdx); // CD+4, 1 last hit key
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);  // cancel list
    // 1st cancel
    disableStoryRelatedReqs(Game.readUInt64(addr + Offsets::Cancel::RequirementsList));
    // 3rd cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 2) + Offsets::Cancel::RequirementsList));

    // 1,1,2
    addr = getMoveAddress(moveset, 0x2226A9EE, idleStanceIdx);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList); // cancel list
    // 3rd cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 2) + Offsets::Cancel::RequirementsList));

    // Juggle Escape
    addr = getMoveAddress(moveset, 0xDEBED999, 5);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList); // cancel list
    // 6th cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 6) + Offsets::Cancel::RequirementsList));
    // 7th cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 7) + Offsets::Cancel::RequirementsList));

    // f,f+2
    addr = getMoveAddress(moveset, 0x1A571FA1, 2000);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList); // cancel list
    // 20th cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 21) + Offsets::Cancel::RequirementsList));
    // 21st cancel
    disableStoryRelatedReqs(Game.readUInt64((addr + Sizes::Moveset::Cancel * 22) + Offsets::Cancel::RequirementsList));

    // d/b+1+2
    addr = getMoveAddress(moveset, 0x73EBDBA2, idleStanceIdx);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList); // cancel list
    // 3rd cancel
    {
      uintptr_t req0 = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
      Game.write<uintptr_t>((addr + Sizes::Moveset::Cancel * 2) + Offsets::Cancel::RequirementsList, req0);
    }

    // d/b+4
    addr = getMoveAddress(moveset, 0x9364E2F5, idleStanceIdx);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);         // cancel list
    addr = Game.readUInt64(addr + Offsets::Cancel::RequirementsList); // req list
    // 1st cancel
    disableStoryRelatedReqs(addr);
    // Disabling standing req
    Game.write<int>(addr + Sizes::Moveset::Requirement, 0);

    Game.writeString(moveset + 8, "ALI");
    return true;
  }
  // Devil-less Kazuya
  if (bossCode == 244)
  {
    // Go through reqs and props to disable his devil form
    // requirements
    uintptr_t start = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
    uintptr_t count = Game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
    for (uintptr_t i = 0; i < count; i++)
    {
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
    for (uintptr_t i = 0; i < count; i++)
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
    addr = Game.readUInt64(addr + Offsets::Move::CancelList) + Sizes::Moveset::Cancel;
    Game.write<int>(addr, 0x10);
    Game.write<short>(addr + Offsets::Cancel::Option, 0x50);

    uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
    // Ultra-wavedash
    addr = getMoveAddress(moveset, 0x77314B09, idleStanceIdx); // 2
    // Replacing 2nd cancel requirement list index
    addr = Game.readUInt64(addr + Offsets::Move::CancelList) + Sizes::Moveset::Cancel;
    Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);

    // CD+1+2
    addr = getMoveAddress(moveset, 0x0C9CE140, idleStanceIdx);
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);
    // Storing the story cancel req
    uintptr_t storyReq = Game.readUInt64(addr + Offsets::Cancel::RequirementsList);
    // Replacing 1st cancel requirement list index
    Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);

    // ff2
    // addr = getMoveAddress(moveset, 0xC00BB85A, idleStanceIdx); // 2
    // 21st cancel
    // addr = Game.readUInt64(addr + TekkenOffsets::Move::CancelList) + (40 * 20);
    // Game.write<uintptr_t>(addr + TekkenOffsets::Cancel::RequirementsList, reqHeader);

    // b+2,2
    addr = getMoveAddress(moveset, 0x8B5BFA6C, idleStanceIdx); // 2nd hit of b+2,2
    // Replacing 1st cancel requirement list index
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);
    Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);

    // NEW b+2,2. Disabling laser cancel
    addr = getMoveAddress(moveset, 0x8FE28C6A, defaultAliasIdx); // 2nd hit of b+2,2
    // Replacing 2nd cancel requirement list index
    addr = Game.readUInt64(addr + Offsets::Move::CancelList) + Sizes::Moveset::Cancel;
    if (Game.readInt32(storyReq) == STORY_BATTLE_REQ)
    {
      Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, storyReq);
    }

    // Disabling u/b+1+2 laser
    // key ub1: 0x1376C644
    // key ub1+2: 0x07F32E0C
    addr = getMoveAddress(moveset, 0x07F32E0C, 2000); // u/b+1+2
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);
    {
      uintptr_t cancelExtradata = Game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasHeader) + Sizes::Moveset::CancelExtradata * 16;
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
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);
    {
      int moveId = Game.readInt16((addr + Sizes::Moveset::Cancel) + Offsets::Cancel::Move);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId);
    }

    // d/b+1, 2
    addr = getMoveAddress(moveset, 0xFE501006, idleStanceIdx); // d/b+1
    addr = Game.readUInt64(addr + Offsets::Move::CancelList);
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
      uintptr_t cancelExtradata = Game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasHeader) + Sizes::Moveset::CancelExtradata * 20;
      addr = getMoveAddress(moveset, 0x73EBDBA2, moveId_db1);
      addr = Game.readUInt64(addr + Offsets::Move::CancelList);
      // Adjusting the 1st cancel
      Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId_db1);
    }

    // ws+2
    addr = getMoveAddress(moveset, 0xB253E5F2, idleStanceIdx);                         // d/b+1
    addr = Game.readUInt64(addr + Offsets::Move::CancelList) + Sizes::Moveset::Cancel; // 2nd cancel
    {
      uintptr_t cancelExtradata = Game.readUInt64(moveset + Offsets::Moveset::CancelExtraDatasHeader) + Sizes::Moveset::CancelExtradata * 20;
      int moveId = getMoveId(moveset, 0x0AB42E52, defaultAliasIdx);
      Game.write<uintptr_t>(addr + Offsets::Cancel::RequirementsList, reqHeader);
      Game.write<uintptr_t>(addr + Offsets::Cancel::CancelExtradata, cancelExtradata);
      Game.write<int>(addr + Offsets::Cancel::WindowStart, 5);
      Game.write<int>(addr + Offsets::Cancel::WindowEnd, 5);
      Game.write<int>(addr + Offsets::Cancel::TransitionFrame, 5);
      Game.write<short>(addr + Offsets::Cancel::Move, (short)moveId);
      Game.write<short>(addr + Offsets::Cancel::Option, 65);
    }

    Game.writeString(moveset + 8, "ALI");
    return true;
  }
  return true;
}

bool loadHeihachi(uintptr_t moveset, int bossCode)
{
  // if (bossCode != 353) return true;
  int defaultAliasIdx = Game.readUInt16(moveset + 0x30);
  int idleStanceIdx = Game.readUInt16(moveset + 0x32);
  uintptr_t addr = 0;
  // Get into idle stance cancels
  if (bossCode == 353)
  {
    addr = getMoveAddressByIdx(moveset, idleStanceIdx);
    addr = Game.readUInt64(addr + Offsets::Move::ExtraPropList); // props
    addr = addr + 4 * Sizes::Moveset::ExtraMoveProperty;         // 5th prop
    Game.write<int>(addr + Offsets::ExtraProp::Prop, 0x83F9);
    Game.write<int>(addr + Offsets::ExtraProp::Value, 1);
  }

  uintptr_t reqHeader = Game.readUInt64(moveset + Offsets::Moveset::RequirementsHeader);
  uintptr_t reqCount = Game.readUInt64(moveset + Offsets::Moveset::RequirementsCount);
  int req = 0, param = 0;
  int targetParam = bossCode - 350;
  for (uintptr_t i = 0; i < reqCount; i++)
  {
    addr = reqHeader + i * Sizes::Moveset::Requirement;
    req = Game.readInt32(addr);
    param = Game.readInt32(addr + 4);
    if ((req == 806 && param == targetParam) || req == 801 || (req == 802 && param >= 2049))
    {
      Game.write<int64_t>(addr, 0);
    }
  }

  Game.writeString(moveset + 8, "ALI");
  return true;
}

bool loadAngelJin(uintptr_t moveset, int bossCode)
{
  // TODO: Try fixing intros/outros
  return true;
}

bool loadTrueDevilKazuya(uintptr_t moveset, int bossCode)
{
  // d/f+1, 2
  uintptr_t addr = getMoveAddress(moveset, 0x4339a4bd, 1673);
  addr = Game.readUInt64(addr + Offsets::Move::CancelList); // cancel address
  addr = addr + Sizes::Moveset::Cancel * 22; // 23rd cancel
  addr = Game.readUInt64(addr + Offsets::Cancel::RequirementsList);
  disableStoryRelatedReqs(addr, 473);
  Game.writeString(moveset + 8, "ALI");
  return true;
}

bool loadStoryDevilJin(uintptr_t moveset, int bossCode)
{
  // TODO: Try fixing intros/outros
  return true;
}

uintptr_t getMoveAddress(uintptr_t moveset, int moveNameKey, int start = 0)
{
  uintptr_t movesHead = Game.readUInt64(moveset + Offsets::Moveset::MovesHeader);
  int movesCount = Game.readInt32(moveset + Offsets::Moveset::MovesCount);
  start = start >= movesCount ? 0 : start;
  for (int i = start; i < movesCount; i++)
  {
    uintptr_t addr = movesHead + i * Sizes::Moveset::Move;
    EncryptedValue *paramAddr = reinterpret_cast<EncryptedValue *>(addr);
    uintptr_t decryptedValue = Game.callFunction<uintptr_t, EncryptedValue>(DECRYPT_FUNC_ADDR, paramAddr);
    if ((int)decryptedValue == moveNameKey)
      return addr;
  }
  return 0;
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
  for (int i = start; i < movesCount; i++)
  {
    EncryptedValue *paramAddr = reinterpret_cast<EncryptedValue *>(movesHead + i * Sizes::Moveset::Move);
    uintptr_t decryptedValue = Game.callFunction<uintptr_t, EncryptedValue>(DECRYPT_FUNC_ADDR, paramAddr);
    if ((int)decryptedValue == moveNameKey)
      return i;
  }
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
