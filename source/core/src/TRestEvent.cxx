#include "TRestEvent.h"

#include "TRestLogManager.h"
#include "TRestRun.h"
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
}

void TRestEvent::InitializeReferences(TRestRun* run) {
    SetRunOrigin(run->GetRunNumber());
    SetSubRunOrigin(run->GetSubRunNumber());
}

void TRestEvent::SetRestRun(TRestRun* run){
  fRestRun = run;
}

void TRestEvent::PrintEvent() const {
    RESTLog << "\nEventID: " << fInfo.eventID << "  SubEventID: " << fInfo.subEventID << RESTendl;
    RESTLog << "  Timestamp: " << TRestTools::GetTimeStampFromUnixTime(GetTime()) << RESTendl;
    if (fInfo.subEventTag[0] != '\0') RESTLog << "  Tag: " << fInfo.subEventTag << RESTendl;
    if (!fInfo.ok) RESTLog << "  [Status NOT OK]" << RESTendl;
}
