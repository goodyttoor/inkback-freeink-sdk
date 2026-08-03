#pragma once

// FreeInk SDK — Xteink X3/X4 runtime detection.
//
// The Xteink X3 and X4 are two BoardProfiles compiled into one ESP32-C3 binary.
// They share a pinout but differ in panel controller (X3 = UC8253 792x528,
// X4 = SSD1677 800x480) and battery backend, so the running firmware must pick
// the right one before bringing up the display and SD card. The SDK leaves
// detection to the consumer (BoardConfig.h header note); this helper supplies
// the canonical Xteink fingerprint so dual X3/X4 apps don't each reinvent it.
//
// Detection probes the X3-only I2C peripherals on SDA=20 / SCL=0 — the BQ27220
// fuel gauge (0x55), DS3231 RTC (0x68) and QMI8658 IMU (0x6B/0x6A). The X4 has
// none of them, so two passes scoring >= 2 hits each confirm an X3; anything
// else is treated as an X4 (the conservative default).
//
// In builds without an Xteink profile (neither FREEINK_DEVICE_X4 nor
// FREEINK_DEVICE_X3) both functions compile to no-ops returning false and never
// touch a pin: the probe bus (SDA=20 / SCL=0) is only safe on the Xteink C3
// pinout — on an ESP32-S3 those are native USB D+ and the boot strap.

#include <stdint.h>

namespace freeink {

// Probe outcome. X3Confirmed / X4Confirmed mean both passes agreed (>= 2 hits
// each, or zero hits each); Inconclusive means the passes disagreed or saw a
// single stray ACK — treat it as an X4 but don't persist the answer, so a
// flaky first boot gets re-probed.
enum class XteinkVerdict : uint8_t { X4Confirmed, X3Confirmed, Inconclusive };

// Run the X3 I2C fingerprint and return the verdict. Optionally reports the
// per-pass chip-hit scores (0-3) for diagnostics. Leaves the I2C bus released
// and the probe pins back in INPUT mode. Safe to call before any other
// hardware bring-up. In builds without an Xteink profile this is a no-op
// returning Inconclusive with zero scores.
XteinkVerdict detectXteinkVerdict(uint8_t* score1 = nullptr, uint8_t* score2 = nullptr);

// Run the X3 I2C fingerprint and return true if this board is an Xteink X3.
// Leaves the I2C bus released and the probe pins back in INPUT mode. Safe to
// call before any other hardware bring-up.
bool detectXteinkIsX3();

// --- X3 display-controller fingerprint ---------------------------------------
// Newer X3 production units ship a UC8279d panel controller instead of the
// UC8253 (same board, glass and pinout). The two are told apart by reading the
// UC8279's VER (0x70: reserved 0x00 + CHIP_VER + 24-bit LUT_VER) and FLG
// (0x71: status, BUSY_N=1 when idle) registers over a bit-banged half-duplex
// 4-wire SPI on the X3 display pins, after a hardware reset pulse. The UC8253
// either doesn't answer 0x70 (bus floats) or answers with a different byte
// shape, so a matching UC8279 signature in two independent passes confirms the
// new controller; anything else conservatively resolves to the shipping
// UC8253. Safe to call before FreeInkDisplay::begin() — the pins are released
// afterwards and the driver re-resets the panel.
enum class X3DisplayVerdict : uint8_t { Uc8253Assumed, Uc8279Confirmed, Inconclusive };

// Probe the X3 display controller. Optionally reports the raw VER bytes and
// FLG byte from the first pass (for bring-up logging / threshold tuning on new
// hardware). Only meaningful on a confirmed X3; in builds without
// FREEINK_DEVICE_X3 this is a no-op returning Uc8253Assumed.
X3DisplayVerdict detectX3DisplayController(uint8_t verBytes[5] = nullptr, uint8_t* flg = nullptr);

// --- Board-agnostic display-controller fingerprint ---------------------------
// Newer production runs of several Xteink panels swap their default controller
// for an UltraChip sibling that shares the UC81xx KW-mode command set: the X3's
// UC8253 -> UC8279d, and the X4 / X4 Pro's SSD1677 -> UC8179. All UC81xx parts
// answer a VER (0x70) / FLG (0x71) read; the SSD-family and UC8253 parts do not
// answer 0x70 the same way, so a matching UC81xx signature in two independent
// passes confirms the sibling silicon. Unlike detectX3DisplayController (which
// hard-codes the X3 C3 pinout), this reads the pins from BoardConfig::ACTIVE,
// so it works on any Xteink profile — including the S3 X4 Pro, where the X3 I2C
// probe would be unsafe. Bit-bangs a half-duplex 4-wire SPI after a reset pulse
// and leaves the pins released; safe to call before FreeInkDisplay::begin().
enum class DisplayControllerVerdict : uint8_t { PrimaryAssumed, Uc81xxConfirmed, Inconclusive };

DisplayControllerVerdict detectXteinkDisplayController(uint8_t verBytes[5] = nullptr, uint8_t* flg = nullptr);

// Which UltraChip sibling an X4 Pro carries, from LUT_VER's most significant
// byte (VER[2]).
//
// PURE, and separated from the promotion for one reason: these four values were
// recovered by disassembling CrossPoint beta 10, not read from a datasheet, and
// one of them is inferred rather than decoded. Constants like that need a test
// standing over them, and a function that touches BoardConfig and Serial cannot
// have one. See docs/inkback/reference/crosspoint-x4pro-beta10.md.
//
//   0x01              -> UC8179
//   0x02, 0x68, 0x69  -> UC8279 800x480
//   anything else     -> Unrecognised, and the caller falls back
//
// Beta 10 logs 0x02 and 0x68 with a ", reserved" caveat — promoted, but not the
// value it expects — which reservedLutVer() reports so our log can say the same.
enum class UltraChipSibling : uint8_t { Uc8179, Uc8279, Unrecognised };

// Defined inline: pure arithmetic over one byte, and this way the host test
// that pins the table needs no translation unit and no Arduino.
inline UltraChipSibling siblingFromLutVer(const uint8_t lutVerMsb) {
  // 0x68 is the ONE inferred entry. Beta 10's range test reads
  // (uint8_t)(v - 0x68) <= 1, but the instruction that performs the comparison
  // does not decode under xtensa-esp-elf-objdump, so this value rests on the
  // surrounding idiom rather than on an opcode. Treated as promoting because
  // that is what the idiom says; flagged reserved because beta 10 flags it.
  if (lutVerMsb == 0x69 || lutVerMsb == 0x68 || lutVerMsb == 0x02) return UltraChipSibling::Uc8279;
  if (lutVerMsb == 0x01) return UltraChipSibling::Uc8179;
  return UltraChipSibling::Unrecognised;
}

// True when the byte promotes but is not the value upstream treats as canonical
// (0x69 for UC8279, 0x01 for UC8179). False for unrecognised bytes too — there
// is nothing to caveat about a value that decided nothing.
inline bool reservedLutVer(const uint8_t lutVerMsb) {
  if (siblingFromLutVer(lutVerMsb) == UltraChipSibling::Unrecognised) return false;
  return lutVerMsb != 0x69 && lutVerMsb != 0x01;
}

// Convenience: resolve which panel controller this unit carries and, when it is
// the UltraChip sibling, promote BoardConfig::ACTIVE.displayController to it
// (SSD1677 -> UC8179, UC8253 -> UC8279) so FreeInkDisplay::begin() selects the
// matching driver. The decision is made from the live display-bus probe
// (detectXteinkDisplayController()) — the ground truth for what silicon is
// actually present. The OEM NVS value (hw_calib/screenType) is read only for
// diagnostics and logged for cross-reference; it is NOT used to decide, because
// it is unreliable in the field (a full-flash from another unit overwrites that
// namespace and can name the wrong panel). Leaves the profile's default
// controller in place when the probe doesn't confirm an UltraChip part. Returns
// true iff the controller was promoted. Call before FreeInkDisplay::begin(). In
// builds without a probe-capable profile this is a no-op returning false.
bool applyXteinkDisplayController();

// What the boot-time probe actually saw, kept so a diagnostic screen can report
// it WITHOUT probing again.
//
// Re-probing at runtime is not an option: detectXteinkDisplayController()
// pulses the panel's reset line before bit-banging the read, which is safe
// before FreeInkDisplay::begin() and decidedly not after it — doing it late
// re-resets the panel mid-session and has frozen an X3 in the field. So the one
// legitimate probe records what it read and anything that wants the numbers
// reads them from here.
//
// The point of exposing them is LUT_VER. CrossPoint beta 10 tells a UC8179 from
// a UC8279 by it; we cannot, because the discriminating values are not
// documented anywhere verifiable. Two units reporting this record — one of each
// sibling — settles it. Until then screenType is the tiebreaker, and it is
// recorded here beside the probe so a disagreement between them is visible
// rather than inferred.
struct DisplayProbeRecord {
  bool valid = false;         // false until applyXteinkDisplayController() has run
  uint8_t ver[5] = {0};       // reserved 0x00, CHIP_VER, LUT_VER[23:0] big-endian
  uint8_t flg = 0;
  uint32_t lutVer = 0;        // LUT_VER[23:0], unpacked from ver[2..4]
  bool ultraChip = false;     // what the probe concluded
  bool screenTypeKnown = false;
  uint8_t screenType = 0;     // OEM NVS hint; info only, never the primary decision
};

const DisplayProbeRecord& lastDisplayProbe();

// Convenience: run detectXteinkIsX3(), set BoardConfig::ACTIVE to the matching
// profile via selectDevice(), and return whether an X3 was detected (so the
// caller can put FreeInkDisplay in X3 mode with setDisplayX3()). On a
// confirmed X3 the display-controller probe also runs, selecting the UC8279
// sibling profile when that controller is fingerprinted. Call this before
// SDCardManager::begin() and FreeInkDisplay::begin() so both read the correct
// profile.
bool selectXteinkDevice();

}  // namespace freeink
