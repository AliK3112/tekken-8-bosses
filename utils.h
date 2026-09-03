#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <stdexcept>

enum BossCodes {
  None = -1,
  RegularJin, // 0
  NerfedJin, // 1
  MishimaJin, // 2
  KazamaJin, // 3
  FinalJin, // 4
  ChainedJin = 11,
  Azazel = 32,
  DevilKazuya = 97,
  AngelJin = 117,
  TrueDevilKazuya = 118,
  DevilJin = 121,
  DevilJin_1 = 1211, // Chapter 1
  DevilJin_2 = 1212, // Chapter 12
  DevilJin_3 = 1213, // Chapter 13
  FinalKazuya = 244,
  AmnesiaHeihachi = 351, // Heihachi (35), Variant # 1
  ShadowHeihachi = 352, // Heihachi (35), Variant # 2
  FinalHeihachi = 353, // Heihachi (35), Variant # 3
};

namespace HudIcon
{
  const char *JinFinal = "ant2";
  const char *DvjCh12 = "swl3";
  const char *DvjCh13 = "swl4";
  const char *KazFinal = "grl2";
  const char *KazDevil = "grl3";
  const char *HeiMonk = "bee2";
  const char *HeiShadow = "bee3";
};

namespace HudName
{
  const char *KazDevil = "grl2";
  const char *HeiShadow = "bee3";
};

// Function to convert a hexadecimal string to uintptr_t
uintptr_t hexStringToUintptr(const std::string &hexStr)
{
  uintptr_t value;
  std::stringstream ss;
  ss << std::hex << hexStr;
  ss >> value;
  if (ss.fail())
  {
    throw std::invalid_argument("Invalid hex value: " + hexStr);
  }
  return value;
}

// Function to read key-value pairs from a file
std::map<std::string, uintptr_t> readKeyValuePairs(const std::string &filename)
{
  std::map<std::string, uintptr_t> keyValuePairs;
  std::ifstream file(filename);
  if (!file)
  {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  std::string line;
  while (std::getline(file, line))
  {
    std::istringstream iss(line);
    std::string key, valueStr;
    if (std::getline(iss, key, '=') && std::getline(iss, valueStr))
    {
      uintptr_t value = hexStringToUintptr(valueStr);
      keyValuePairs[key] = value;
    }
  }

  return keyValuePairs;
}

uintptr_t getValueByKey(const std::map<std::string, uintptr_t> &config, const std::string &key)
{
  auto it = config.find(key);
  if (it != config.end())
  {
    return it->second;
  }
  else
  {
    throw std::runtime_error("Key not found: " + key);
  }
}

static constexpr size_t HUD_PATH_MAX = 255;

void buildIconPath(char *out, size_t outSize, char side, const char *code)
{
  if (!out || outSize == 0)
    return;
  if (!code)
    code = "";
  snprintf(out, outSize, "T_UI_HUD_Character_Icon_%c_%s", side, code);
}

void buildNamePath(char *out, size_t outSize, const char *code)
{
  if (!out || outSize == 0)
    return;
  if (!code)
    code = "";
  snprintf(out, outSize, "T_UI_HUD_Character_Name_%s", code);
}

void getIconPath(char *out, size_t outSize, int side, int charId)
{
  buildIconPath(out, outSize, side == 0 ? 'L' : 'R', getCharCode(charId));
}

// This is an override
void buildNamePath(char *out, size_t outSize, int charId)
{
  buildNamePath(out, outSize, getCharCode(charId));
}

std::string getBossName(int bossCode)
{
  switch (bossCode)
  {
  case BossCodes::RegularJin: return "Jin - Boosted";
  case BossCodes::NerfedJin: return "Jin - Karate";
  case BossCodes::ChainedJin: return "Jin - Chained";
  case BossCodes::MishimaJin: return "Jin - Mishima";
  case BossCodes::KazamaJin: return "Jin - Kazama";
  case BossCodes::FinalJin: return "Jin - Final";
  case BossCodes::DevilKazuya: return "Devil Kazuya";
  case BossCodes::FinalKazuya: return "Kazuya - Final";
  case BossCodes::AmnesiaHeihachi: return "Heihachi - Monk";
  case BossCodes::ShadowHeihachi: return "Heihachi - Shadow";
  case BossCodes::FinalHeihachi: return "Heihachi - Final";
  case BossCodes::AngelJin: return "Angel Jin";
  case BossCodes::TrueDevilKazuya: return "True Devil Kazuya";
  case BossCodes::DevilJin: return "Devil Jin";
  case BossCodes::DevilJin_1: return "Devil Jin - Chapter 1";
  case BossCodes::DevilJin_2: return "Devil Jin - Chapter 12";
  case BossCodes::DevilJin_3: return "Devil Jin - Chapter 13";
  case BossCodes::Azazel: return "Azazel";
  default: return "__unknown__";
  }
}

void printAddr(const std::string &name, uintptr_t addr)
{
  printf("%s: 0x%llX\n", name.c_str(), (unsigned long long)addr);
}
