#include "TRestEvent.h"

#include "TRestLogManager.h"
#include "TRestTools.h"

void TRestEvent::SetTime(double time) {
    auto seconds = static_cast<int>(time);
    auto nanoseconds = static_cast<int>((time - seconds) * 1e9);
    fInfo.timeSeconds = seconds;
    fInfo.timeNanoSeconds = nanoseconds;
}

void TRestEvent::SetEventInfo(const TRestEvent* eve) {
    if (!eve) return;
    fInfo = eve->fInfo;
    fSubEventTag = eve->fSubEventTag;
}

void TRestEvent::PrintEvent() const {
    RESTLog << "\nEventID: " << fInfo.eventID << "  SubEventID: " << fInfo.subEventID << RESTendl;
    RESTLog << "  Timestamp: " << TRestTools::GetTimeStampFromUnixTime(GetTime()) << RESTendl;
    if (!fSubEventTag.empty()) RESTLog << "  Tag: " << fSubEventTag << RESTendl;
    if (!fInfo.ok) RESTLog << "  [Status NOT OK]" << RESTendl;
}
