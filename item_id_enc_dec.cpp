#include <windows.h>
#include <string>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <vector>
#include <cstdarg>
#include <process.h>
#include "game.h"
#include "tekken.h"

const char CLASS_NAME[] = "EncryptorWindow";

// TODO: Handle edge cases
// This works for 28-bit values with 4-bit checksum (both enc and dec)
// But not for 32-bit values with ???-bit checksum (both end and dec)

HWND hwndInput, hwndHexRadio, hwndDecRadio, hwndEncryptBtn, hwndDecryptBtn, hwndOutput, hwndLogBox;
GameClass game;
uintptr_t DECRYPT_FUNC_ADDR = 0;
uintptr_t ENCRYPT_FUNC_ADDR = 0;
bool ATTACHED = false;
int INPUT_MODE = 0; // 0 = Hex, 1 = Dec
std::string ENC_BYTES = "48 89 5C 24 08 48 89 7C 24 10 44 8B 09 48 BF BA 0F A4 DC 96 FB CC ED 41 0F B6 C1 48 8B D9 4C 8B C7 24 1F 76 1C 0F B6 D0 0F 1F 84 00 00 00 00 00 49 8B C0 48 C1 E8 3F 4E 8D 04 40 48 83 EA 01 75 EF 41 83 E0 E0 45 33 DB 45 33 C1 45 8B CB 41 83 F0 1D 41 81 E0 FF FF FF 00 45 8B D0 0F 1F 40 00 41 0F B6 C9 48 8B C7 80 C1 08 74 15 0F B6 D1 90 48 8B C8 48 C1 E9 3F 48 8D 04 41 48 83 EA 01 75 EF 41 33 C2 41 83 C1 08 44 33 D8 41 C1 FA 08 41 83 F9 18 72 CB 48 8B 7C 24 10 45 84 DB 41 0F B6 C3 B9 01 00 00 00 0F 44 C1 C1 E0 18 41 03 C0 89 03 48 8B 5C 24 08 C3";
std::string DEC_BYTES = "40 57 48 83 EC 20 8B 39 8B CF E8 ?? ?? ?? ?? 3B C7 75 5F 83 F7 1D 48 89 5C 24 30 40 0F B6 C7 48 BB BA 0F A4 DC 96 FB CC ED 24 1F 76 14 0F B6 C8 48 8B C3 48 C1 E8 3F 48 8D 1C 58 48 83 E9 01 75 EF F3 0F 10 0D ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 E3 E0 33 DF C1 E3 08 8B C3 48 8B 5C 24 30 F3 0F 2C C8 99 F7 F9 48 83 C4 20 5F C3 33 C0 48 83 C4 20 5F C3";

void InitializeUI(HWND hwnd);
void AppendLog(const std::string &msg);
void AppendLog(const char *format, ...);
void SetInputMode();
void AttachToGame();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
unsigned int __stdcall AttachToGameThread(void *param);
void HandleEncrypt();
void HandleDecrypt();
int GetParsedInput();
void UpdateOutputField();
void scanAddresses();
uint32_t encryptValue(uint32_t input);
uint32_t TK__pack24BitWith8BitChecksum(uint32_t input);
uint32_t TK__validateAndTransform24Bit(uint32_t* pInput);
float TK__safeNormalizedPow(float x, float y);
uint64_t TK__generateAndValidate28BitWith4BitChecksum(uint32_t* output, uint32_t input);
uint32_t TK__pack28BitWith4BitChecksum(uint32_t input);
uint64_t TK__validateAndTransform28BitWith4BitChecksum(uint32_t* pInput);
uint32_t TK__encrypt28BitWith4BitChecksum(uint32_t plaintext);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
  WNDCLASSA wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInst;
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(CLASS_NAME, "Encryption Tool",
                            WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
                            NULL, NULL, hInst, NULL);
  if (!hwnd)
    return 0;

  InitializeUI(hwnd);
  ShowWindow(hwnd, nCmdShow);

  // HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, AttachToGameThread, NULL, 0, NULL);
  // if (hThread)
  //   CloseHandle(hThread);

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
  int padding = 20, spacing = 10, btnWidth = 100, btnHeight = 25;
  int labelWidth = 360, fieldWidth = 360, fieldHeight = 20;

  hwndInput = CreateWindowA("EDIT", "",
                            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                            padding, padding, fieldWidth, fieldHeight,
                            hwnd, NULL, NULL, NULL);

  hwndHexRadio = CreateWindowA("BUTTON", "Hexadecimal", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                               padding, padding + fieldHeight + spacing, 120, 20, hwnd, (HMENU)1, NULL, NULL);
  hwndDecRadio = CreateWindowA("BUTTON", "Decimal", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                               padding + 130, padding + fieldHeight + spacing, 120, 20, hwnd, (HMENU)2, NULL, NULL);
  SendMessageA(hwndHexRadio, BM_SETCHECK, BST_CHECKED, 0);

  hwndEncryptBtn = CreateWindowA("BUTTON", "Encrypt", WS_CHILD | WS_VISIBLE,
                                 padding, padding + fieldHeight + 2 * spacing + 20, btnWidth, btnHeight,
                                 hwnd, (HMENU)3, NULL, NULL);
  hwndDecryptBtn = CreateWindowA("BUTTON", "Decrypt", WS_CHILD | WS_VISIBLE,
                                 padding + btnWidth + spacing, padding + fieldHeight + 2 * spacing + 20, btnWidth, btnHeight,
                                 hwnd, (HMENU)4, NULL, NULL);

  hwndOutput = CreateWindowA("EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                             padding, padding + fieldHeight + 3 * spacing + 45, fieldWidth, fieldHeight,
                             hwnd, NULL, NULL, NULL);

  hwndLogBox = CreateWindowA("EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                             padding, padding + fieldHeight + 4 * spacing + 70, fieldWidth, 80,
                             hwnd, NULL, NULL, NULL);
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

void SetInputMode()
{
  if (SendMessageA(hwndHexRadio, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    INPUT_MODE = 0; // Hexadecimal
  }
  else
  {
    INPUT_MODE = 1; // Decimal
  }
}

int GetParsedInput()
{
  char inputBuffer[255];
  GetWindowTextA(hwndInput, inputBuffer, sizeof(inputBuffer));
  std::string inputStr(inputBuffer);

  int result = 0;
  std::istringstream iss(inputStr);
  if (SendMessageA(hwndHexRadio, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    if (!(iss >> std::hex >> result))
      return -1;
  }
  else
  {
    if (!(iss >> std::dec >> result))
      return -1;
  }
  return result;
}

void HandleEncrypt()
{
  EnableWindow(hwndInput, FALSE);
  uint32_t value = GetParsedInput();
  if (value == -1)
  {
    SetWindowTextA(hwndOutput, "Invalid Input");
  }
  else
  {
    uint32_t encVal = TK__encrypt28BitWith4BitChecksum(value);
    char buffer[32];

    if (SendMessageA(hwndHexRadio, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
      sprintf_s(buffer, "0x%X", encVal);
    }
    else
    {
      sprintf_s(buffer, "%u", encVal);
    }

    // Test with in-code stuff
    // {
    //   uint32_t encrypted = TK__encrypt28BitWith4BitChecksum(value);
    //   printf("Encrypted (code): 0x%.8x\n", encrypted);
    //   printf("Encrypted (game): 0x%.8x\n", encryptValue(value));
    // }

    SetWindowTextA(hwndOutput, buffer);
  }
  EnableWindow(hwndInput, TRUE);
}

void HandleDecrypt()
{
  EnableWindow(hwndInput, FALSE);
  int value = GetParsedInput();
  if (value == -1)
  {
    SetWindowTextA(hwndOutput, "Invalid Input");
  }
  else
  {
    std::string decryptedValue = "[Decrypted]";
    if (true)
    {
      try
      {
        // uint32_t param = (uint32_t)value;
        // uint32_t decrypted = game.callFunction<uint32_t, uint32_t>(DECRYPT_FUNC_ADDR, &param, true);

        uint32_t* param = new uint32_t;
        *param = (uint32_t)value;
        uint32_t decrypted = TK__validateAndTransform28BitWith4BitChecksum(param);
        delete param;
        
        if (INPUT_MODE == 0)
        {
          std::stringstream stream;
          stream << std::hex << decrypted;
          decryptedValue = "0x" + stream.str();
        }
        else if (INPUT_MODE == 1)
        {
          decryptedValue = std::to_string(decrypted);
        }
        // printf("Decrypted (game): 0x%.8x\n", decrypted);

        // Test with in-code method
        // {
        //   uint32_t* param = new uint32_t;
        //   *param = (uint32_t)value;
        //   uint32_t decrypted = TK__validateAndTransform28BitWith4BitChecksum(param);
        //   printf("Decrypted (code): 0x%.8x\n", decrypted);
        //   decryptedValue = std::to_string(decrypted);
        // }
      }
      catch (...)
      {
        SetWindowTextA(hwndOutput, "Some Error Occured");
      }
    }
    SetWindowTextA(hwndOutput, decryptedValue.c_str());
  }
  EnableWindow(hwndInput, TRUE);
}

void AttachToGame()
{
  AppendLog("Waiting for game to start...");
  while (true)
  {
    if (game.Attach(L"Polaris-Win64-Shipping.exe"))
    {
      AppendLog("Successfully attached to game!");
      ATTACHED = true;
      break;
    }
    Sleep(1000);
  }
  scanAddresses();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
  case WM_COMMAND:
    switch (LOWORD(wp))
    {
    case 1: // Hex Radio Button
    case 2: // Dec Radio Button
      SetInputMode();
      UpdateOutputField();
      break;
    case 3: // Encrypt Button
      HandleEncrypt();
      break;
    case 4: // Decrypt Button
      HandleDecrypt();
      break;
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

void UpdateOutputField()
{
  char buffer[255];
  GetWindowTextA(hwndOutput, buffer, sizeof(buffer));
  std::string outputStr(buffer);

  int value = 0;

  if (outputStr.find("0x") == 0) // Check if it's in hex format
  {
    std::stringstream ss;
    ss << std::hex << outputStr.substr(2); // Remove "0x" prefix and convert
    ss >> value;
  }
  else
  {
    std::stringstream ss(outputStr);
    ss >> value;
  }

  if (SendMessageA(hwndHexRadio, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    std::stringstream stream;
    stream << std::hex << value;
    std::string result = "0x" + stream.str();
    SetWindowTextA(hwndOutput, result.c_str());
  }
  else
  {
    SetWindowTextA(hwndOutput, std::to_string(value).c_str());
  }
}

unsigned int __stdcall AttachToGameThread(void *param)
{
  AttachToGame();
  return 0;
}

void scanAddresses()
{
  // uintptr_t addr = 0;
  // uintptr_t base = game.getBaseAddress();
  // uintptr_t start = base;
  // addr = game.FastAoBScan(DEC_BYTES, base + 0x1700000);
  // if (addr != 0)
  // {
  //   DECRYPT_FUNC_ADDR = addr;
  //   AppendLog("Decryption Function Address: 0x%llx", addr);
  // }
  // else
  // {
  //   DECRYPT_FUNC_ADDR = 0;
  //   AppendLog("Decryption Function Address not found!");
  // }

  // addr = game.FastAoBScan(ENC_BYTES, base + 0x1700000);
  // if (addr != 0)
  // {
  //   ENCRYPT_FUNC_ADDR = addr;
  //   AppendLog("Encryption Function Address: 0x%llx", addr);
  // }
  // else
  // {
  //   ENCRYPT_FUNC_ADDR = 0;
  //   AppendLog("Encryption Function Address not found!");
  // }
}

uint32_t encryptValue(uint32_t input)
{
  uint64_t v2 = 0xEDCCFB96DCA40FBA;

  // Step 1: Bitwise shift-carry transformation
  if (input & 0x1F)
  {
    uint8_t shiftCount = input & 0x1F;
    for (uint8_t i = 0; i < shiftCount; i++)
    {
      v2 = (v2 >> 63) + (v2 << 1);
    }
  }

  // Step 2: XOR and mask with 0xFFFFFFE0
  uint32_t v5 = input ^ (v2 & 0xFFFFFFE0);

  // Step 3: Apply additional XOR with 0x1D and mask with 0xFFFFFF
  uint32_t v7 = (v5 ^ 0x1D) & 0xFFFFFF;
  uint32_t v8 = v7;

  // Step 4: Loop through shifts and XOR operations
  uint8_t v4 = 0;
  uint32_t v6 = 0;

  do
  {
    uint64_t v9 = 0xEDCCFB96DCA40FBA;
    uint8_t shiftCount = (v6 & 0xFF) + 8;

    for (uint8_t i = 0; i < shiftCount; i++)
    {
      v9 = (v9 >> 63) + (v9 << 1);
    }

    v6 += 8;
    v4 ^= (uint8_t)v8 ^ (uint8_t)v9;
    v8 >>= 8;
  } while (v6 < 0x18);

  // Step 5: If v4 is 0, set it to 1
  uint32_t v11 = v4 ? v4 : 1;

  // Step 6: Compute the final encrypted value
  uint32_t encrypted = v7 + (v11 << 24);

  return encrypted;
}

// ========================================================
// Pack 24-bit value with 8-bit checksum
// ========================================================

uint32_t TK__pack24BitWith8BitChecksum(uint32_t input) 
{
    const uint64_t kMagic = 0xEDCCFB96DCA40FBA;
    uint8_t checksum = 0;
    
    uint32_t remaining_bits = input & 0xFFFFFF; // Keep only 24 bits
    
    for (uint32_t bit_offset = 0; bit_offset < 24; bit_offset += 8) 
    {
        // Rotate magic constant based on position
        uint64_t rotated_magic = kMagic;
        uint8_t rotate_count = bit_offset + 8; // 8, 16, 24
        
        while (rotate_count--) {
            rotated_magic = (rotated_magic >> 63) | (rotated_magic << 1); // Rotate left
        }
        
        // Mix current byte into checksum
        uint8_t current_byte = remaining_bits & 0xFF;
        checksum ^= current_byte ^ static_cast<uint8_t>(rotated_magic);
        
        remaining_bits >>= 8; // Process next byte
    }
    
    // Ensure checksum is never 0
    if (checksum == 0) checksum = 1;
    
    // Pack into [checksum:8][input:24]
    return (input & 0xFFFFFF) | (checksum << 24);
}

// ========================================================
// Validate and transform 24-bit value
// ========================================================

uint32_t TK__validateAndTransform24Bit(uint32_t* pInput) 
{
    const uint64_t kMagic = 0xEDCCFB96DCA40FBA;
    const uint32_t kXorKey = 0x1D;
    
    // Step 1: Validate checksum
    uint32_t input = *pInput;
    if (TK__pack24BitWith8BitChecksum(input) != input) {
        return 0; // Validation failed
    }
    
    // Step 2: Initial transformation
    uint32_t transformed = input ^ kXorKey;
    
    // Step 3: Magic constant rotation
    uint64_t magic = kMagic;
    uint32_t rotate_bits = transformed & 0x1F; // Lower 5 bits
    
    while (rotate_bits--) {
        magic = (magic >> 63) | (magic << 1); // Rotate left
    }
    
    // Step 4: Final mixing
    uint32_t mixed = (transformed ^ (magic & 0xFFFFFFE0)) << 8;
    
    // Step 5: Normalization
    float normalizer = TK__safeNormalizedPow(2.0f, 8.0f); // 2^8 for 24-bit
    return static_cast<uint32_t>(mixed / normalizer);
}

// ========================================================
// Optimized power function for normalization
// ========================================================

float TK__safeNormalizedPow(float base, float exponent) 
{
    if (base == 1.0f) return 1.0f;
    if (exponent == 0.0f) return 1.0f;
    if (base == 0.0f) return (exponent < 0) ? INFINITY : 0.0f;
    
    // Fast approximation avoids std::pow
    return exp2f(exponent * log2f(base));
}

uint64_t TK__generateAndValidate28BitWith4BitChecksum(uint32_t* output, uint32_t input) {
    const uint64_t kMagic = 0xEDCCFB96DCA40FBA;
    
    // 1. Rotate magic based on lower 5 bits
    uint64_t magic = kMagic;
    uint32_t rotate_bits = input & 0x1F;
    while (rotate_bits--) {
        magic = (magic >> 63) | (magic << 1); // Rotate left
    }

    // 2. Initial mixing
    uint32_t transformed = (input ^ (magic & 0xFFFFFFE0) ^ 0x1D) & 0xFFFFFFF;

    // 3. Compute 4-bit checksum
    uint8_t checksum = 0;
    uint32_t temp = transformed;
    for (int i = 0; i < 7; i++) { // 7 nibbles (28 bits)
        uint64_t rotated = kMagic;
        for (int j = 0; j < 4*(i+1); j++) { // Position-dependent rotation
            rotated = (rotated >> 63) | (rotated << 1);
        }
        checksum ^= (temp & 0xF) ^ (rotated & 0xF);
        temp >>= 4;
    }
    if (!checksum) checksum = 1;

    // 4. Pack and validate
    *output = transformed | (checksum << 28);
    return TK__validateAndTransform24Bit(output);
}

// ========================================================
// Pack 28-bit value with 4-bit checksum
// ========================================================

uint32_t TK__pack28BitWith4BitChecksum(uint32_t input) 
{
    const uint64_t kMagic = 0xEDCCFB96DCA40FBA;
    uint8_t checksum = 0;
    uint32_t remaining_bits = input & 0x0FFFFFFF; // Keep only 28 bits
    
    // Process 7 nibbles (4 bits each)
    for (uint32_t bit_offset = 0; bit_offset < 28; bit_offset += 4) {
        // Rotate magic constant based on position
        uint64_t rotated_magic = kMagic;
        uint8_t rotate_count = bit_offset + 4; // 4, 8, 12,...,28
        
        while (rotate_count--) {
            rotated_magic = (rotated_magic >> 63) | (rotated_magic << 1); // Rotate left
        }
        
        // Mix current nibble into checksum
        uint8_t current_nibble = remaining_bits & 0xF;
        checksum ^= current_nibble ^ (rotated_magic & 0xF);
        
        remaining_bits >>= 4;
    }
    
    // Ensure checksum is never 0
    if (checksum == 0) checksum = 1;
    
    // Pack into [checksum:4][input:28]
    return (input & 0x0FFFFFFF) | ((checksum & 0xF) << 28);
}

// ========================================================
// Validate and transform 28-bit value
// ========================================================

uint64_t TK__validateAndTransform28BitWith4BitChecksum(uint32_t* pInput) 
{
    const uint64_t kMagic = 0xEDCCFB96DCA40FBA;
    const uint32_t kXorKey = 0x1D;
    const float kNormalizer = 4.0f; // 2^2 (for 28-bit input)
    
    // Step 1: Validate checksum
    uint32_t input = *pInput;
    if (TK__pack28BitWith4BitChecksum(input) != input) {
        return 0; // Validation failed
    }
    
    // Step 2: Initial transformation
    uint32_t transformed = input ^ kXorKey;
    
    // Step 3: Magic constant rotation
    uint64_t magic = kMagic;
    uint32_t rotate_bits = transformed & 0x1F; // Lower 5 bits
    
    while (rotate_bits--) {
        magic = (magic >> 63) | (magic << 1); // Rotate left
    }
    
    // Step 4: Final mixing and normalization
    uint32_t mixed = (transformed ^ (magic & 0xFFFFFFE0)) << 4; // Scale up
    float normalized = mixed / TK__safeNormalizedPow(2.0f, kNormalizer);
    
    return static_cast<uint64_t>(normalized);
}


uint32_t TK__encrypt28BitWith4BitChecksum(uint32_t plaintext) {
    const uint64_t kMagic = 0xEDCCFB96DCA40FBA;
    const uint32_t kXorKey = 0x1D;
    
    // Step 1: Scale up and prepare for transformation (reverse of final step)
    uint32_t scaled = static_cast<uint32_t>(plaintext * 16.0f); // Reverse of /16.0
    
    // Step 2: Reverse the mixing
    uint64_t magic = kMagic;
    uint32_t rotate_bits = (scaled >> 4) & 0x1F; // Extract rotation bits
    
    // Rotate magic constant
    while (rotate_bits--) {
        magic = (magic >> 63) | (magic << 1);
    }
    
    // Reverse the XOR mixing
    uint32_t transformed = (scaled >> 4) ^ (magic & 0xFFFFFFE0);
    
    // Step 3: Apply initial XOR (reverse of final step)
    uint32_t pre_checksum = transformed ^ kXorKey;
    
    // Step 4: Generate valid checksum
    return TK__pack28BitWith4BitChecksum(pre_checksum);
}