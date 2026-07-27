#pragma once

// FreeInk SDK — ghost budget.
//
// Differential (fast/DU) refreshes are quick but leave residue. Enough of them
// in a row and the panel visibly greys. The fix is to promote one refresh in
// every N to a full clearing waveform, so the residue never accumulates far
// enough to see.
//
// It8951Driver has done exactly this since it gained a differential path, via
// ghostClearInterval and _partialsSinceClear. This is that logic lifted out so
// other drivers — SSD1677 in particular, which has a windowed path and no
// budget at all — can share it instead of each growing their own copy.
//
// Deliberately knows nothing about RefreshMode, panels or buses: it takes two
// booleans and returns one. That keeps it testable on the host, and keeps the
// mapping from a driver's own refresh modes where it belongs, in the driver.

#include <stdint.h>

namespace freeink {

class GhostBudget {
 public:
  // interval == 0 disables the periodic clear. The other triggers still apply.
  explicit GhostBudget(uint16_t interval = 0) : _interval(interval) {}

  // Should this refresh use the clearing waveform?
  //
  // Three triggers, matching the IT8951 behaviour this generalises:
  //   - the caller asked for something stronger than a differential refresh;
  //   - the panel is not running (a wake needs a fresh image, not a delta);
  //   - the budget is spent.
  // Plus one the drivers cannot detect for themselves: an explicit
  // requestClear(), which is how a consumer forces a clean on an activity
  // transition.
  //
  // Const: deciding and recording are separate so a driver that ends up
  // clearing for its own reasons can still report the truth to recordRefresh().
  bool wouldClear(bool differentialRequested, bool panelRunning) const;

  // Record what actually happened. Resets the counter on a clear, otherwise
  // spends one unit of budget.
  void recordRefresh(bool cleared);

  // Force the next refresh to clear. Survives until a refresh is recorded,
  // so a consumer can set it at any point before the paint.
  void requestClear() { _clearRequested = true; }

  uint16_t partialsSinceClear() const { return _partialsSinceClear; }
  bool clearRequested() const { return _clearRequested; }
  uint16_t interval() const { return _interval; }
  void setInterval(const uint16_t interval) { _interval = interval; }

  // Back to a just-cleared state: no residue assumed, no pending request.
  void reset();

 private:
  uint16_t _interval;
  uint16_t _partialsSinceClear = 0;
  bool _clearRequested = false;
};

}  // namespace freeink
