// Header file for reading a game class that works of an injected DLL

#include <vector>
typedef unsigned long DWORD;

class InjectedGameClass
{
private:
  uintptr_t baseAddress;

public:
  InjectedGameClass() : baseAddress(0) {}

  ~InjectedGameClass() {}

  uintptr_t getBaseAddress()
  {
    return baseAddress;
  }

  void setBaseAddress(uintptr_t baseAddress)
  {
    this->baseAddress = baseAddress;
  }

  uintptr_t getAddress(uintptr_t *offsets, int size)
  {
    uintptr_t address = baseAddress;
    for (int i = 0; i < size; i++)
    {
      address = read<uintptr_t>(address + offsets[i]);
    }
    return address;
  }

  uintptr_t getAddress(const std::vector<DWORD> &offsets)
  {
    uintptr_t address = baseAddress;
    for (DWORD offset : offsets)
    {
      address = read<uintptr_t>(address + (uintptr_t)offset);
    }
    return address;
  }

  template <typename ReturnType, typename ParamType>
  ReturnType callFunction(uintptr_t functionAddress, ParamType *params)
  {
    using FunctionType = ReturnType (*)(ParamType *);
    FunctionType func = reinterpret_cast<FunctionType>(functionAddress);
    return func(params);
  }

  template <typename T>
  void write(uintptr_t address, T value)
  {
    *reinterpret_cast<T *>(address) = value;
  }

  template <typename T>
  T read(uintptr_t address)
  {
    T value;
    T *ptr = reinterpret_cast<T *>(address);

    // Attempt to read memory at the given address
    try
    {
      value = *ptr;
    }
    catch (...)
    {
      // std::cerr << "Error: Failed to read memory at address " << std::hex << address << std::endl;
      value = T(); // Default-initialize to handle errors gracefully
    }

    // If the size of `value` does not match the expected size, log an incomplete read
    if (sizeof(value) != sizeof(T))
    {
      // std::cerr << "Error: Incomplete read at address " << std::hex << address << std::endl;
    }

    return value;
  }

  char readByte(uintptr_t address)
  {
    return read<char>(address);
  }

  uint16_t readUInt16(uintptr_t address)
  {
    return read<uint16_t>(address);
  }

  int16_t readInt16(uintptr_t address)
  {
    return read<int16_t>(address);
  }

  uint32_t readUInt32(uintptr_t address)
  {
    return read<uint32_t>(address);
  }

  int readInt32(uintptr_t address)
  {
    return read<int>(address);
  }

  uint64_t readUInt64(uintptr_t address)
  {
    return read<uint64_t>(address);
  }

  int64_t readInt64(uintptr_t address)
  {
    return read<int64_t>(address);
  }
};