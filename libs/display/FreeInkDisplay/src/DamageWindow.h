#pragma once

// FreeInk SDK — framebuffer damage detection.
//
// Diffs a frame against the previous one and returns the bounding box of what
// changed, so a driver can refresh a window instead of the whole panel.
//
// Panel-agnostic on purpose. Ed2208M5Driver has carried its own copy of this
// since it was the only driver with a windowed path; that copy also folds in
// the M5's logical-to-panel rotation, which is why it could not simply be
// shared. This works entirely in framebuffer coordinates and leaves any
// coordinate mapping to the caller.
//
// Alignment is not optional for every panel: SSD1677 rejects a window whose x
// or width is not a multiple of 8, because its RAM is addressed in bytes. The
// bounding box is therefore always byte-aligned on the x axis, which is also
// why the scan works at byte granularity there — sub-byte precision would be
// discarded anyway.

#include <stdint.h>

namespace freeink {

struct DamageRect {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
};

// Computes the bounding box of the pixels that differ between `fb` and `prev`.
//
// Both buffers must have the same layout: `heightPx` rows of `widthBytes`,
// 1 bit per pixel, MSB first. `padPx` grows the box on every side before
// clamping, which hides the fringing an e-ink partial update leaves at the edge
// of a refreshed region; pass 0 for an exact box.
//
// Returns false when the frames are identical, when either pointer is null, or
// when the geometry is degenerate — in all of those cases `*out` is untouched
// and the caller should fall back to a full refresh rather than refreshing an
// empty window.
//
// The returned rect is always byte-aligned: `x % 8 == 0` and `w % 8 == 0`.
bool computeDamageWindow(const uint8_t* fb, const uint8_t* prev, uint16_t widthPx, uint16_t heightPx,
                         uint16_t widthBytes, uint16_t padPx, DamageRect* out);

}  // namespace freeink
