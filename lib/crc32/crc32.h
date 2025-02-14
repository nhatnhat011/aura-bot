#ifndef AURA_CRC32_H_
#define AURA_CRC32_H_

#include <cstdlib>
#include <cstdint>

namespace CRC32
{
  // Slice-by-16 algorithm.
  constexpr std::size_t MaxSlices = 16;

  extern uint32_t LUT[MaxSlices][256];
  extern bool initialized;

  void Initialize();
  uint32_t Reflect(uint32_t ulReflect, const uint8_t cChar);
  [[nodiscard]] uint32_t CalculateCRC(const uint8_t* data, std::size_t length, uint32_t previous_crc = 0);
};

#endif // AURA_CRC32_H_
