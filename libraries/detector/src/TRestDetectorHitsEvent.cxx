#include "TRestDetectorHitsEvent.h"
#include "TRestSystemOfUnits.h"

#include "TRandom.h"

#include <numeric>

namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestDetectorHitsEvent",
                                       []() { return std::make_unique<TRestDetectorHitsEvent>(); });
    return true;
}();
}  // namespace

void TRestDetectorHitsEvent::AddHit(Double_t x, Double_t y, Double_t z, Double_t en, Double_t t,
                                    REST_HitType type) {
    fHitsData.x.push_back((float)x);
    fHitsData.y.push_back((float)y);
    fHitsData.z.push_back((float)z);
    fHitsData.time.push_back((float)t);
    fHitsData.energy.push_back((float)en);
    fHitsData.type.push_back((int)type);
    RebuildHitsView();
}

void TRestDetectorHitsEvent::AddHit(const XYZVector& position, Double_t energy, Double_t time,
                                    REST_HitType type) {
    AddHit(position.X(), position.Y(), position.Z(), energy, time, type);
}

void TRestDetectorHitsEvent::Sort(std::function<bool(int, int)> compareCondition) {
    fHits.Sort(compareCondition);
    RebuildHitsView();
}

void TRestDetectorHitsEvent::Shuffle(int NLoop) {
    fHits.Shuffle(NLoop);
    RebuildHitsView();
}

TRestHits* TRestDetectorHitsEvent::GetXZHits() {
    fXZData.clear();
    for (unsigned int i = 0; i < GetNumberOfHits(); i++) {
        if (GetType(i) == REST_HitType::XZ) {
            fXZData.x.push_back((float)GetX(i));
            fXZData.y.push_back((float)GetY(i));
            fXZData.z.push_back((float)GetZ(i));
            fXZData.time.push_back((float)GetTime(i));
            fXZData.energy.push_back((float)GetEnergy(i));
            fXZData.type.push_back((int)REST_HitType::XZ);
        }
    }
    fXZHitsView = TRestHits(&fXZData, 0, (int)fXZData.x.size());
    return &fXZHitsView;
}

TRestHits* TRestDetectorHitsEvent::GetYZHits() {
    fYZData.clear();
    for (unsigned int i = 0; i < GetNumberOfHits(); i++) {
        if (GetType(i) == REST_HitType::YZ) {
            fYZData.x.push_back((float)GetX(i));
            fYZData.y.push_back((float)GetY(i));
            fYZData.z.push_back((float)GetZ(i));
            fYZData.time.push_back((float)GetTime(i));
            fYZData.energy.push_back((float)GetEnergy(i));
            fYZData.type.push_back((int)REST_HitType::YZ);
        }
    }
    fYZHitsView = TRestHits(&fYZData, 0, (int)fYZData.x.size());
    return &fYZHitsView;
}

TRestHits* TRestDetectorHitsEvent::GetXYZHits() {
    fXYZData.clear();
    for (unsigned int i = 0; i < GetNumberOfHits(); i++) {
        if (GetType(i) == REST_HitType::XYZ) {
            fXYZData.x.push_back((float)GetX(i));
            fXYZData.y.push_back((float)GetY(i));
            fXYZData.z.push_back((float)GetZ(i));
            fXYZData.time.push_back((float)GetTime(i));
            fXYZData.energy.push_back((float)GetEnergy(i));
            fXYZData.type.push_back((int)REST_HitType::XYZ);
        }
    }
    fXYZHitsView = TRestHits(&fXYZData, 0, (int)fXYZData.x.size());
    return &fXYZHitsView;
}

void TRestDetectorHitsEvent::PrintDetectorHitsEvent(Int_t nHits) const {
    TRestEvent::PrintEvent();

    std::cout << "Total energy : " << GetTotalEnergy() << std::endl;
    std::cout << "Mean position : ( " << REST_Units::FormatAs(GetMeanPosition(), REST_Units::Length) << std::endl;
    std::cout << "Number of hits : " << GetNumberOfHits() << std::endl;
    if (nHits != -1) {
        std::cout << "+++++++++++++++++++++++" << std::endl;
        std::cout << "Printing " << nHits << "out of  "<< GetNumberOfHits() << " hits" << std::endl;
    }

    fHits.PrintHits(nHits);
}
