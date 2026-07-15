#include "TRestRawSignalAnalysisProcess.h"
#include "TRestPulseShapeAnalysis.h"
#include <TObjString.h>
#include <iostream>
#include <limits>

using namespace std;
using namespace TRestPulseShapeAnalysis;

static const bool TRestRawSignalAnalysisProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawSignalAnalysisProcess>("baselineRange", &TRestRawSignalAnalysisProcess::fBaselineRange);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("integralRange", &TRestRawSignalAnalysisProcess::fIntegralRange);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("baselineOption", &TRestRawSignalAnalysisProcess::fBaselineOption);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("pointsThreshold", &TRestRawSignalAnalysisProcess::fPointsThreshold);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("signalThreshold", &TRestRawSignalAnalysisProcess::fSignalThreshold);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("pointsOverThreshold", &TRestRawSignalAnalysisProcess::fPointsOverThreshold);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("signalsRange", &TRestRawSignalAnalysisProcess::fSignalsRange);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("nPointsFlat", &TRestRawSignalAnalysisProcess::fNPointsFlat);
    reg.RegisterField<TRestRawSignalAnalysisProcess>("maxOption", &TRestRawSignalAnalysisProcess::fMaxOption);

    return true;
}();

// Registration in MetadataClassRegistry
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawSignalAnalysisProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawSignalAnalysisProcess>(instanceName, params);
        });
    return true;
}();
}

TRestRawSignalAnalysisProcess::TRestRawSignalAnalysisProcess() : TRestEventProcess() {
    fName = "TRestRawSignalAnalysisProcess";
}

TRestRawSignalAnalysisProcess::TRestRawSignalAnalysisProcess(const std::string& instanceName, const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawSignalAnalysisProcess::TRestRawSignalAnalysisProcess(const std::string& fileName, const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawSignalAnalysisProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull() ) {
        RESTError << "TRestRawSignalAnalysisProcess::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawSignalAnalysisProcess>(fNode);
    //Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestRawSignalAnalysisProcess>(fNode);
}

void TRestRawSignalAnalysisProcess::InitProcess() {
    if (fSignalsRange.first != -1 && fSignalsRange.second != -1) {
        fRangeEnabled = true;
    }

    RegisterObservable("SignalsID",              fObs.SignalsID);
    RegisterObservable("Baseline",               fObs.Baseline);
    RegisterObservable("BaselineSigma",          fObs.BaselineSigma);
    RegisterObservable("AmpSgnMaxMethod",        fObs.AmpSgnMaxMethod);
    RegisterObservable("AmpSgnIntMethod",        fObs.AmpSgnIntMethod);
    RegisterObservable("RiseTime",               fObs.RiseTime);
    RegisterObservable("RiseSlope",              fObs.RiseSlope);
    RegisterObservable("PeakTime",               fObs.PeakTime);
    RegisterObservable("PointsOverThr",          fObs.PointsOverThr);
    RegisterObservable("Saturated",              fObs.Saturated);

    RegisterObservable("BaselineAvg",            fObs.BaselineAvg);
    RegisterObservable("BaselineSigmaAvg",       fObs.BaselineSigmaAvg);
    RegisterObservable("TimeBinLength",          fObs.TimeBinLength);
    RegisterObservable("NSignals",               fObs.NSignals);
    RegisterObservable("NGoodSignals",           fObs.NGoodSignals);
    RegisterObservable("FullIntegralSum",        fObs.FullIntegralSum);
    RegisterObservable("ThresholdIntegralSum",   fObs.ThresholdIntegralSum);
    
    RegisterObservable("RiseSlopeAvg",           fObs.RiseSlopeAvg);
    RegisterObservable("SlopeIntegral",          fObs.SlopeIntegral);
    RegisterObservable("RateOfChangeAvg",        fObs.RateOfChangeAvg);
    RegisterObservable("RiseTimeAvg",            fObs.RiseTimeAvg);
    RegisterObservable("MaxIntegral",            fObs.MaxIntegral);
    RegisterObservable("IntegralBalance",        fObs.IntegralBalance);
    RegisterObservable("AmplitudeIntegralRatio", fObs.AmplitudeIntegralRatio);
    RegisterObservable("MinPeakAmplitude",       fObs.MinPeakAmplitude);
    RegisterObservable("MaxPeakAmplitude",       fObs.MaxPeakAmplitude);
    RegisterObservable("PeakAmplitudeIntegral",  fObs.PeakAmplitudeIntegral);
    RegisterObservable("MinEventValue",          fObs.MinEventValue);
    RegisterObservable("AmplitudeRatio",         fObs.AmplitudeRatio);
    RegisterObservable("MaxPeakTime",            fObs.MaxPeakTime);
    RegisterObservable("MinPeakTime",            fObs.MinPeakTime);
    RegisterObservable("MaxPeakTimeDelay",       fObs.MaxPeakTimeDelay);
    RegisterObservable("AveragePeakTime",        fObs.AveragePeakTime);

}

bool TRestRawSignalAnalysisProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {

    output.CopyFrom(&input);
    
    auto* sigEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!sigEvent) {
        throw std::runtime_error("TRestRawSignalAnalysisProcess: output is not a TRestRawSignalEvent");
    }

    //Reset observable values
    fObs = SignalObservables{};
    const auto thrParams = std::make_pair(fPointsThreshold, fSignalThreshold);

    fObs.MaxPeakAmplitude = 0.0;
    fObs.MinPeakAmplitude = std::numeric_limits<double>::max();
    fObs.MinEventValue    = std::numeric_limits<double>::max();

    fObs.MinPeakTime      = std::numeric_limits<double>::max();
    fObs.MaxPeakTime      = 0.0;
    fObs.AveragePeakTime  = 0.0;


      for (int s = 0; s < sigEvent->GetNumberOfSignals(); s++) {
        const auto &signal = sigEvent->GetSignal(s);
        const int sID = signal.GetSignalID();
        if (fRangeEnabled && (sID < fSignalsRange.first || sID > fSignalsRange.second))
        continue;
        
        double baseline, baselineSigma;
        auto data = CalculateBaselineAndSigma(signal.GetData(), fBaselineRange.first, fBaselineRange.second, baseline, baselineSigma, fBaselineOption);
        
        auto pointsOverThr = GetPointsOverThreshold(data, fIntegralRange, thrParams, fPointsOverThreshold, fNPointsFlat, baselineSigma);

        fObs.NSignals++;
        if(pointsOverThr.size() > fPointsThreshold)fObs.NGoodSignals++;

        int thrIntegral = 0;
        for (const auto& [bin, value] : pointsOverThr) {
            thrIntegral += value;
        }

        const std::string title = "Signal ID: " + std::to_string(sID);
        auto sGraph = GetGraphPair(pointsOverThr,title);
        const auto maxPeak = GetMax(sGraph, fMaxOption);
        std::pair<double,double> minVal {std::numeric_limits<double>::max(),std::numeric_limits<double>::max()};
        if(pointsOverThr.size() > 0 )minVal = GetMin(sGraph);
        const auto riseTime = GetRiseTime(pointsOverThr);
        const auto riseSlope = GetRiseSlope(pointsOverThr);
        bool saturated = signal.GetPoint(maxPeak.first) >= 4096;

        fObs.SignalsID.push_back(sID);
        fObs.Baseline.push_back(baseline);
        fObs.BaselineSigma.push_back(baselineSigma);
        fObs.AmpSgnMaxMethod.push_back(maxPeak.second);
        fObs.PeakTime.push_back(maxPeak.first);
        fObs.AmpSgnIntMethod.push_back(thrIntegral);
        fObs.RiseTime.push_back(riseTime);
        fObs.RiseSlope.push_back(riseSlope);
        fObs.PointsOverThr.push_back(pointsOverThr.size());
        fObs.Saturated.push_back(saturated);

        if(pointsOverThr.size() > fPointsThreshold){
          fObs.FullIntegralSum += GetIntegral(data, fIntegralRange.first , fIntegralRange.second);
          fObs.ThresholdIntegralSum += thrIntegral;
          fObs.MaxIntegral += maxPeak.second;
          fObs.SlopeIntegral += GetSlopeIntegral(pointsOverThr);
          if (maxPeak.second > fObs.MaxPeakAmplitude) fObs.MaxPeakAmplitude = maxPeak.second;
          if (maxPeak.second < fObs.MinPeakAmplitude) fObs.MinPeakAmplitude = maxPeak.second;

          fObs.AveragePeakTime += maxPeak.first;
          if (maxPeak.first < fObs.MinPeakTime) fObs.MinPeakTime = maxPeak.first;
          if (maxPeak.first > fObs.MaxPeakTime) fObs.MaxPeakTime = maxPeak.first;
          if (minVal.second < fObs.MinEventValue) {
            fObs.MinEventValue = minVal.second;
          }
        }        

      }

   if(fObs.NSignals <=0)return true;
   fObs.BaselineAvg = std::accumulate(fObs.Baseline.begin(), fObs.Baseline.end(), 0.0)/fObs.NSignals;
   fObs.BaselineSigmaAvg = std::accumulate(fObs.BaselineSigma.begin(), fObs.BaselineSigma.end(), 0.0)/fObs.NSignals;

   auto calcAvgOverThr = [](const auto& data, const std::vector<int>& pointsThr, double threshold) -> double {
    if (data.size() != pointsThr.size()) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if (pointsThr[i] > threshold) {
            sum += data[i];
            count++;
        }
    }
    return (count > 0) ? (sum / count) : 0.0;
};

   fObs.RiseSlopeAvg = calcAvgOverThr(fObs.RiseSlope, fObs.PointsOverThr, fPointsThreshold);
   fObs.RiseTimeAvg = calcAvgOverThr(fObs.RiseTime, fObs.PointsOverThr, fPointsThreshold);
   fObs.TimeBinLength = sigEvent->GetSignal(0).GetNPoints();
   fObs.RateOfChangeAvg = fObs.RiseSlopeAvg/fObs.SlopeIntegral;
   fObs.IntegralBalance = (fObs.FullIntegralSum - fObs.ThresholdIntegralSum) / (fObs.FullIntegralSum + fObs.ThresholdIntegralSum);
   
   if (fObs.NGoodSignals > 0) {
     fObs.AveragePeakTime /= fObs.NGoodSignals;
     fObs.MaxPeakTimeDelay = fObs.MaxPeakTime - fObs.MinPeakTime;
   } else {
    fObs.MinPeakAmplitude = 0.0;
    fObs.MinEventValue    = 0.0;
    fObs.MinPeakTime      = 0.0;
  }

    return true;
}

void TRestRawSignalAnalysisProcess::EndProcess() {

}


