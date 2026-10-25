#ifndef TRESTTRACKLINEANALYSISPROCESS_H
#define TRESTTRACKLINEANALYSISPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestTrackLineAnalysisProcess
/// \brief Origin, end, length and direction of linear tracks (e.g. alpha tracks), for any readout: from
/// the most energetic track or, for the given projections (hit types, e.g. XZ and YZ for strips), from
/// the main tracks of all of them together. The output event holds the tracks used.
class TRestTrackLineAnalysisProcess : public TRestEventProcess {
   private:
    /// Main track of one projection (hit type)
    struct ProjectionTracks {
        std::string name;
        int type = 0;
        int index = -1;  ///< Track index in the event, -1 if none
        double maxEnergy = 0;
        double energy = 0;     ///< Of the tracks of this type
        double balance = 0;    ///< maxEnergy / energy
        double length = 0;     ///< Of its main track, from its own origin to its own end
        double angle = 0;      ///< Polar angle of its main track, NaN without Z
        double asymmetry = 0;  ///< Energy asymmetry of the halves of its main track
        double residual = 0;   ///< Distance of its main track to a line
    };

    std::vector<ProjectionTracks> fProjectionTracks;

    double fOriginX = 0, fOriginY = 0, fOriginZ = 0;
    double fEndX = 0, fEndY = 0, fEndZ = 0;
    double fLength = -1;
    double fAngle = 0;
    double fPhi = 0;
    int fDownwards = 0;
    double fEnergy = 0;
    double fRelativeZ = 0;
    double fTrackBalance = 0;
    double fAsymmetry = 0;
    double fResidual = 0;

    /// The warnings for an unknown method, or "3D" without a common coordinate, are given once
    bool fMethodWarned = false;

   public:
    /// Projections (hit types: X, Y, Z, XY, XZ, YZ, XYZ) whose main tracks form the track, e.g. [XZ, YZ]
    /// for strip readouts. Empty: the most energetic track, whatever its type
    std::vector<std::string> fProjections = {};

    /// Origin and end with several projections: "default" (ends of each one) or "3D" (nodes paired)
    std::string fLineAnaMethod = "default";

    TRestTrackLineAnalysisProcess();
    TRestTrackLineAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    TRestTrackLineAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackLineAnalysisProcess"; }
};

#endif
