#include "DamageWindow.h"

#include <string.h>

namespace freeink {

bool computeDamageWindow(const uint8_t* fb, const uint8_t* prev, const uint16_t widthPx, const uint16_t heightPx,
                         const uint16_t widthBytes, const uint16_t padPx, DamageRect* out) {
  if (!fb || !prev || !out) return false;
  if (widthPx == 0 || heightPx == 0 || widthBytes == 0) return false;

  uint16_t minByte = widthBytes;
  uint16_t maxByte = 0;
  uint16_t minRow = heightPx;
  uint16_t maxRow = 0;
  bool any = false;

  for (uint16_t row = 0; row < heightPx; row++) {
    const uint32_t rowOffset = static_cast<uint32_t>(row) * widthBytes;
    // Whole-row memcmp first. On a page turn most rows differ, but on the
    // updates a windowed refresh is actually for — a cursor, a status bar, a
    // selection — nearly every row is identical, and this skips them at
    // memcmp speed instead of byte-comparing the panel width.
    if (memcmp(fb + rowOffset, prev + rowOffset, widthBytes) == 0) continue;

    any = true;
    if (row < minRow) minRow = row;
    maxRow = row;  // rows ascend, so this is always the latest differing row

    // Only the columns outside the box found so far can widen it.
    for (uint16_t b = 0; b < minByte; b++) {
      if (fb[rowOffset + b] != prev[rowOffset + b]) {
        minByte = b;
        break;
      }
    }
    for (uint16_t b = widthBytes; b > maxByte + 1u; b--) {
      const uint16_t idx = b - 1;
      if (fb[rowOffset + idx] != prev[rowOffset + idx]) {
        maxByte = idx;
        break;
      }
    }
  }

  if (!any) return false;

  // Pad, in whole bytes on x so the result stays byte-aligned.
  const uint16_t padBytes = static_cast<uint16_t>((padPx + 7u) / 8u);
  const uint16_t x0 = (minByte > padBytes) ? static_cast<uint16_t>(minByte - padBytes) : 0;
  const uint16_t x1 = (static_cast<uint32_t>(maxByte) + padBytes < widthBytes)
                          ? static_cast<uint16_t>(maxByte + padBytes)
                          : static_cast<uint16_t>(widthBytes - 1);
  const uint16_t y0 = (minRow > padPx) ? static_cast<uint16_t>(minRow - padPx) : 0;
  const uint16_t y1 = (static_cast<uint32_t>(maxRow) + padPx < heightPx) ? static_cast<uint16_t>(maxRow + padPx)
                                                                        : static_cast<uint16_t>(heightPx - 1);

  out->x = static_cast<uint16_t>(x0 * 8u);
  out->w = static_cast<uint16_t>((x1 - x0 + 1) * 8u);
  out->y = y0;
  out->h = static_cast<uint16_t>(y1 - y0 + 1);

  // widthBytes may cover more pixels than widthPx when the panel width is not a
  // multiple of 8 (the X3 is 792, which is; a future panel might not be). Never
  // report a window past the visible edge.
  if (out->x >= widthPx) return false;
  if (static_cast<uint32_t>(out->x) + out->w > widthPx) {
    out->w = static_cast<uint16_t>(widthPx - out->x);
  }
  return true;
}

}  // namespace freeink
