#include "TRestSystemOfUnits.h"

#include <TGeoManager.h>

namespace REST_Units {

bool SetGeoManagerDefaultUnits() {
    static const bool done = [] {
        // ROOT locks the default units once geometry elements exist
        TGeoManager::LockDefaultUnits(kFALSE);
        TGeoManager::SetDefaultUnits(TGeoManager::kG4Units);
        TGeoManager::LockDefaultUnits(kTRUE);
        return true;
    }();
    return done;
}

// Also applied when the library is loaded (e.g. from plain ROOT sessions).
static const bool kGeoManagerDefaultUnitsSet = SetGeoManagerDefaultUnits();

}  // namespace REST_Units
