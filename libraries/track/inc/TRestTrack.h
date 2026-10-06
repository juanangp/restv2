#ifndef TRESTTRACK_H
#define TRESTTRACK_H

#include <Math/Vector3D.h>
#include <Rtypes.h>
#include <vector>
#include <functional>
#include "TRestHits.h"

class TRestTrackEvent;

class TRestTrack {
   protected:
    TRestTrackEvent* fEvent = nullptr;
    std::size_t fTrackIndex = 0;

    bool IsValid() const;
    std::size_t GetHitStart() const;
    std::size_t GetHitCount() const;

   public:
    using XYZVector = ROOT::Math::XYZVector;

    TRestTrack() = default;
    TRestTrack(TRestTrackEvent* event, std::size_t index);
    ~TRestTrack() = default;

    const TRestTrackEvent* GetEvent() const { return fEvent; }
    void SetEvent(TRestTrackEvent* event) { fEvent = event; }
    std::size_t GetIndex() const { return fTrackIndex; }

    const TRestHits GetHits() const;
    Int_t GetTrackID() const;
    Int_t GetParentID() const;
    Int_t GetNumberOfHits() const;
    Double_t GetEnergy() const;
    Double_t GetLength() const;
    Double_t GetVolume() const;
    
    void GetBoundaries(XYZVector& orig, XYZVector& end) const;
    static void ComputeBoundaries(const TRestHits& hits, XYZVector& orig, XYZVector& end);

    std::vector<float> GetSigmaX() const;
    std::vector<float> GetSigmaY() const;
    std::vector<float> GetSigmaZ() const;
    float GetSigmaX(std::size_t localHitIdx) const;
    float GetSigmaY(std::size_t localHitIdx) const;
    float GetSigmaZ(std::size_t localHitIdx) const;

    friend class TRestTrackEvent;
};

#endif

