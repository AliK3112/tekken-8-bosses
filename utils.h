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
  FinalKazuya = 244,
  AmnesiaHeihachi = 351, // Heihachi (35), Variant # 1
  ShadowHeihachi = 352, // Heihachi (35), Variant # 2
  FinalHeihachi = 353, // Heihachi (35), Variant # 3
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

std::string buildString(const char side, const std::string& code) {
    std::stringstream ss;
    ss << "T_UI_HUD_Character_Icon_" << side << "_" << code;
    return ss.str();
}

std::string getIconPath(int side, int charId)
{
  return buildString(side == 0 ? 'L' : 'R', getCharCode(charId));
}

std::string getNamePath(int charId)
{
  std::stringstream ss;
  ss << "T_UI_HUD_Character_Name_" << getCharCode(charId);
  return ss.str();
}

std::string getNamePath(const std::string& code)
{
  std::stringstream ss;
  ss << "T_UI_HUD_Character_Name_" << code;
  return ss.str();
}

std::string getBossName(int bossCode)
{
  switch (bossCode)
  {
  case BossCodes::RegularJin: return "Jin (Boosted)";
  case BossCodes::NerfedJin: return "Jin (Nerfed)";
  case BossCodes::ChainedJin: return "Jin (Chained)";
  case BossCodes::MishimaJin: return "Jin (Mishima)";
  case BossCodes::KazamaJin: return "Jin (Kazama)";
  case BossCodes::FinalJin: return "Jin (Final)";
  case BossCodes::DevilKazuya: return "Kazuya (Devil)";
  case BossCodes::FinalKazuya: return "Kazuya (Final)";
  case BossCodes::AmnesiaHeihachi: return "Heihachi (Monk)";
  case BossCodes::ShadowHeihachi: return "Heihachi (Shadow)";
  case BossCodes::FinalHeihachi: return "Heihachi (Final)";
  case BossCodes::AngelJin: return "Jin (Angel)";
  case BossCodes::TrueDevilKazuya: return "Kazuya (True Devil)";
  case BossCodes::DevilJin: return "Jin (Devil)";
  case BossCodes::Azazel: return "Azazel";
  default: return "__unknown__";
  }
}
