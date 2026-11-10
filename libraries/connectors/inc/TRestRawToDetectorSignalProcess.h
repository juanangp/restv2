#ifndef TRESTRAWTODETECTORSIGNALPROCESS_H
#define TRESTRAWTODETECTORSIGNALPROCESS_H

#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"

class TRestDetectorReadout;

/// \class TRestRawToDetectorSignalProcess
/// \brief Transforms a TRestRawSignalEvent (ADC samples per bin) into a TRestDetectorSignalEvent
/// (time, amplitude points), optionally applying zero suppression.
///
/// \author Javier Gracia
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawToDetectorSignalProcess : public TRestEventProcess {
   private:
    /// Number of signals of the event without any point in the output
    int fNSignalsRejected = 0;

    /// Readout used for SignalsPhysicalID, nullptr without TRestDetectorReadoutManager
    const TRestDetectorReadout* fReadout = nullptr;

    /// DAQ channel (signal ID) of each output signal
    std::vector<int> fSignalsID;

    /// Physical (readout) channel of each output signal, -1 if its DAQ channel is not in the readout
    std::vector<int> fSignalsPhysicalID;

    /// Maximum amplitude of the points of each output signal
    std::vector<double> fSignalsMaxAmplitude;

   public:
    /// Sampling time used to transform the bin number into time
    TRestWithUnits fSampling = 0.02;

    /// Time of the first bin of the raw signal
    double fTriggerStarts = 0;

    /// Factor applied to the amplitude of the output points
    double fGain = 1;

    /// Minimum raw value of a point to be transferred (only without zero suppression)
    double fThreshold = 0.1;

    /// If true, only the points over threshold (see TRestRawSignalAnalysisProcess) are transferred,
    /// with the baseline subtracted
    bool fZeroSuppression = false;

    /// Bin range used to calculate the baseline and its fluctuation (zero suppression)
    std::pair<int, int> fBaselineRange = {5, 55};

    /// Baseline calculation option (zero suppression), see TRestPulseShapeAnalysis
    std::string fBaselineOption = "";

    /// Bin range where the points over threshold are searched (zero suppression)
    std::pair<int, int> fIntegralRange = {100, 500};

    /// Number of baseline sigmas for a point to be over threshold (zero suppression)
    double fPointsThreshold = 3;

    /// Number of baseline sigmas for a pulse to be accepted (zero suppression)
    double fSignalThreshold = 5;

    /// Number of consecutive points over threshold to accept a pulse (zero suppression)
    int fPointsOverThreshold = 5;

    /// Number of flat points that end a pulse over threshold (zero suppression)
    int fNPointsFlat = 512;

    /// Name of the readout (in TRestDetectorReadoutManager) for SignalsPhysicalID. It can be omitted if
    /// the manager contains a single readout (the data readout)
    std::string fReadoutName = "";

    /// \brief Default constructor, with the default parameters.
    TRestRawToDetectorSignalProcess();
    /// \brief Constructor from the YAML section `node` of the instance `instanceName` (calls LoadConfig).
    TRestRawToDetectorSignalProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from the section `sectionName` of the YAML file `fileName` (calls LoadConfig).
    TRestRawToDetectorSignalProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered fields from the YAML section and normalizes baselineOption.
    void LoadConfig() override;
    /// \brief Registers the observables and, if the manager has a TRestDetectorReadoutManager, selects the
    /// readout and registers SignalsPhysicalID (requesting it without readouts throws).
    void InitProcess() override;
    /// \brief Transfers the points of each raw signal (see the class description).
    /// \return False if no signal has points.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawToDetectorSignalProcess"; }
};

#endif
