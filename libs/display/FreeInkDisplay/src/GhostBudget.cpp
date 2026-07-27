#include "GhostBudget.h"

namespace freeink {

bool GhostBudget::wouldClear(const bool differentialRequested, const bool panelRunning) const {
  if (!differentialRequested) return true;  // caller asked for a stronger refresh
  if (!panelRunning) return true;           // waking: the panel needs a whole image
  if (_clearRequested) return true;         // consumer forced one, e.g. activity change
  return _interval != 0 && _partialsSinceClear >= _interval;
}

void GhostBudget::recordRefresh(const bool cleared) {
  _clearRequested = false;
  if (cleared) {
    _partialsSinceClear = 0;
    return;
  }
  // Saturate rather than wrap. Wrapping would silently hand back a full budget
  // at the exact moment the panel is dirtiest, which is the failure this class
  // exists to prevent.
  if (_partialsSinceClear != UINT16_MAX) _partialsSinceClear++;
}

void GhostBudget::reset() {
  _partialsSinceClear = 0;
  _clearRequested = false;
}

}  // namespace freeink
