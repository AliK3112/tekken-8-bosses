#pragma once

// Rebuilds the fighter-id -> 3-letter-code map from the game's read-only string
// data, so fighters released after this trainer was built still resolve to the
// right HUD icon and name paths.
//
// The codes live in a run of 4-byte-aligned 3-char literals in fighter-id
// order, opening at Paul ("GRF"/"grf") and closing at Seiryu ("XXG"/"xxg"):
//
//   +0x00 "GRF"  +0x04 "grf"  +0x08 "PIG"  +0x0C "pig"
//   +0x10 "pgn"  +0x14 "CML"  +0x18 "cml"  +0x1C "SNK"  ...
//
// Each fighter contributes an upper form, a lower form, or both. King's "PGN"
// is absent because the linker pooled that literal with a copy elsewhere in the
// module, so the walk lowercases every slot and collapses consecutive
// duplicates rather than assuming a fixed stride.
//
// Nothing is published unless the scan reproduces every code getCharCode()
// already hardcodes. A fighter losing both forms to pooling would shift every
// id after it and wreck the "dek"/"xxa".."xxg" tail, so that comparison
// doubles as a checksum over the entire run.

#include "game.h"
#include "tekken.h"

namespace Tekken
{
  namespace CharCodeScan
  {
    const std::string ANCHOR_SIG = "47 52 46 00 67 72 66 00"; // "GRF\0grf\0"
    const std::string ID_JUMP_CODE = "dek";                   // restarts ids at FighterId::Dummy
    const std::string LIST_END_CODE = "xxg";

    constexpr size_t SLOT_SIZE = 4; // "abc\0"
    constexpr size_t CODE_LEN = 3;
    constexpr size_t MAX_SLOTS = 1024;
    constexpr size_t NULL_RUN_TERMINATOR = 4;
    constexpr int MIN_VALIDATED_CODES = 45;

    // The table sits well past the midpoint of the module; scanning from here
    // first saves a few seconds, with a full sweep as the fallback.
    constexpr uintptr_t SCAN_HINT_RVA = 0x7000000;

    // A slot holds "ABC\0" or "abc\0" with an alphanumeric, single-case body.
    bool readSlot(const uint8_t *slot, std::string &lowerOut)
    {
      if (slot[CODE_LEN] != 0)
        return false;

      std::string lower;
      for (size_t i = 0; i < CODE_LEN; i++)
      {
        const uint8_t c = slot[i];
        if (c >= 'A' && c <= 'Z')
          lower.push_back(static_cast<char>(c + 0x20));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
          lower.push_back(static_cast<char>(c));
        else
          return false;
      }

      lowerOut = lower;
      return true;
    }

    size_t leadingNullRun(const uint8_t *data, size_t available)
    {
      size_t run = 0;
      while (run < available && data[run] == 0)
        run++;
      return run;
    }

    std::vector<uint8_t> readBlock(GameClass &game, uintptr_t address, size_t byteCount)
    {
      for (size_t n = byteCount; n >= 256; n /= 2)
      {
        std::vector<uint8_t> block = game.readArray<uint8_t>(address, n);
        if (!block.empty())
          return block;
      }
      return {};
    }

    std::map<int, std::string> walk(GameClass &game, uintptr_t tableStart)
    {
      std::map<int, std::string> codes;

      std::vector<uint8_t> block = readBlock(game, tableStart, MAX_SLOTS * SLOT_SIZE);
      if (block.empty())
        return codes;

      int nextId = 0;
      std::string previous;

      for (size_t offset = 0; offset + SLOT_SIZE <= block.size(); offset += SLOT_SIZE)
      {
        const uint8_t *slot = block.data() + offset;

        if (leadingNullRun(slot, block.size() - offset) >= NULL_RUN_TERMINATOR)
          break;

        std::string code;
        if (!readSlot(slot, code))
          break;

        if (code == previous)
          continue; // the same fighter's other case

        previous = code;

        if (code == ID_JUMP_CODE)
          nextId = FighterId::Dummy;

        codes[nextId++] = code;

        if (code == LIST_END_CODE)
          break;
      }

      return codes;
    }
  }

  // Scans the module and publishes any fighter getCharCode() does not already
  // know. Returns how many were published, or -1 if the scan failed validation,
  // in which case nothing is published and the hardcoded table stands alone.
  int scanFighterCodes(GameClass &game, std::string &status)
  {
    // Validation compares against the hardcoded switch, so drop anything a
    // previous scan published before asking getCharCode() what it knows.
    scannedCharCodes.clear();

    const uintptr_t base = game.getBaseAddress();
    uintptr_t tableStart = game.FastAoBScan(CharCodeScan::ANCHOR_SIG, base + CharCodeScan::SCAN_HINT_RVA);
    if (tableStart == 0)
      tableStart = game.FastAoBScan(CharCodeScan::ANCHOR_SIG);

    if (tableStart == 0)
    {
      status = "Fighter code table not found (using built-in codes)";
      return -1;
    }

    std::map<int, std::string> scanned = CharCodeScan::walk(game, tableStart);

    int confirmed = 0;
    std::vector<int> newcomers;

    for (const std::pair<const int, std::string> &entry : scanned)
    {
      const std::string known = getCharCode(entry.first);
      if (known == "Unknown")
      {
        newcomers.push_back(entry.first);
        continue;
      }

      if (known != entry.second)
      {
        status = "Fighter code scan rejected: id " + std::to_string(entry.first) +
                 " reads \"" + entry.second + "\", expected \"" + known + "\" (using built-in codes)";
        return -1;
      }

      confirmed++;
    }

    const bool endpointsIntact = scanned.count(FighterId::Paul) &&
                                 scanned.count(FighterId::Dummy) &&
                                 scanned.count(FighterId::Seiryu);

    if (!endpointsIntact || confirmed < CharCodeScan::MIN_VALIDATED_CODES)
    {
      status = "Fighter code scan rejected: only " + std::to_string(confirmed) +
               " codes confirmed (using built-in codes)";
      return -1;
    }

    char summary[192];
    snprintf(summary, sizeof(summary), "Fighter codes at rva 0x%llX: %d confirmed, %d new",
             static_cast<unsigned long long>(tableStart - base), confirmed, static_cast<int>(newcomers.size()));
    status = summary;

    for (size_t i = 0; i < newcomers.size(); i++)
    {
      const int id = newcomers[i];
      scannedCharCodes[id] = scanned[id];
      status += (i == 0 ? " (" : ", ") + std::to_string(id) + "=\"" + scanned[id] + "\"";
    }
    if (!newcomers.empty())
      status += ")";

    return static_cast<int>(newcomers.size());
  }
}
