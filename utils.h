#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <stdexcept>

enum BossCodes {
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
  AmnesiaHeihachi = 351, // 35, 1
  ShadowHeihachi = 352, // 35, 2
  FinalHeihachi = 353, // 35, 3
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
