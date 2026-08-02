#pragma once

// UC8279 TRES (command 0x61) resolution encoding, as a pure function.
//
// WHY THIS IS ITS OWN HEADER. The driver used to write the resolution as four
// literal bytes — 0x03 0x18 0x02 0x10, which is 792x528, the X3 panel it was
// written for. Every other dimension in that driver already came from
// BoardConfig::ACTIVE, so the buffers were panel-agnostic while the CONTROLLER
// was told a fixed size. On an 800x480 panel that programs the wrong scan
// geometry, and the symptom is a shifted or compressed image rather than an
// error anyone could log.
//
// That matters now because beta 9 documents the UC8279 on the X4 Pro at
// 800x480, not only on the X3 at 792x528.
//
// Pure, dependency-free and header-only so a host test can check the arithmetic
// without a panel — which is the only way to check it at all until hardware
// arrives.
//
// FORMAT, from the datasheet: two big-endian 16-bit values, HRES then VRES.
// Horizontal resolution is 8-pixel granular, so the low three bits of HRES are
// zero; masking rather than assuming keeps a mis-specified width from silently
// programming a fractional byte.

#include <cstdint>

namespace freeink {

struct Uc8279Resolution {
  uint8_t hresHigh;
  uint8_t hresLow;
  uint8_t vresHigh;
  uint8_t vresLow;
};

// The four TRES payload bytes for a panel of the given size.
//
// Byte-identical to the literals it replaces when handed 792x528, which is the
// property that makes this safe to land without an X3 to test on.
constexpr Uc8279Resolution uc8279Resolution(const uint16_t width, const uint16_t height) {
  return Uc8279Resolution{
      static_cast<uint8_t>((width >> 8) & 0x03u),
      // Low three bits cleared: horizontal resolution is 8-pixel granular.
      static_cast<uint8_t>(width & 0xF8u),
      static_cast<uint8_t>((height >> 8) & 0x03u),
      static_cast<uint8_t>(height & 0xFFu),
  };
}

}  // namespace freeink
