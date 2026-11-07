#include "TRestEvent.h"

#include <cmath>

#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

void TRestEvent::SetTime(double time) {
    // Whole seconds (floor, also for negative times, beyond the range of int) and nanoseconds in [0, 1e9)
    const double seconds = std::floor(time);
    auto nanoseconds = static_cast<long long>(std::llround((time - seconds) * 1e9));
    auto wholeSeconds = static_cast<long long>(seconds);
    if (nanoseconds >= 1000000000LL) {
        nanoseconds -= 1000000000LL;
        ++wholeSeconds;
    }
    fInfo.timeSeconds = wholeSeconds;
    fInfo.timeNanoSeconds = static_cast<int>(nanoseconds);
}

void TRestEvent::SetEventInfo(const TRestEvent* eve) {
    if (!eve) return;
    fInfo = eve->fInfo;
}

void TRestEvent::InitializeReferences(TRestRun* run) {
    SetRunOrigin(run->GetRunNumber());
    SetSubRunOrigin(run->GetSubRunNumber());
}

void TRestEvent::SetRestRun(TRestRun* run) { fRestRun = run; }

void TRestEvent::PrintEvent() const {
    RESTLog << "\nEventID: " << fInfo.eventID << "  SubEventID: " << fInfo.subEventID << RESTendl;
    RESTLog << "  Timestamp: " << TRestTools::GetTimeStampFromUnixTime(GetTime()) << RESTendl;
    if (fInfo.subEventTag[0] != '\0') RESTLog << "  Tag: " << fInfo.subEventTag << RESTendl;
    if (!fInfo.ok) RESTLog << "  [Status NOT OK]" << RESTendl;
}
