#ifndef TRESTDATASETSIGNALSELECTION_H
#define TRESTDATASETSIGNALSELECTION_H

#include <string>
#include <utility>

#include "TRestDataSetProcess.h"

/// \class TRestDataSetSignalSelection
/// \brief Selection of the entries of a dataset in the range of an observable holding a quantile interval of
/// the signal (the reference), e.g. the 80% of a calibration with the lowest log odds. See the class
/// documentation in TRestDataSetSignalSelection.cxx.
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDataSetSignalSelection : public TRestDataSetProcess {
   protected:
    /// \brief The range of the observable between the quantiles of the reference, and the fraction of the
    /// reference in it. Throws if the reference has no entries.
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;

   public:
    /// Observable (column of the dataset)
    std::string fObservable = "";
    /// Quantile interval [lower, upper] of the reference kept (0 and 1: no limit on that side)
    std::pair<double, double> fQuantiles = {0, 1};

    /// Result: range of the observable [min, max] between the quantiles (-inf, inf: no limit)
    std::pair<double, double> fRange = {0, 0};
    /// Result: fraction of the reference in the range (the signal efficiency)
    double fEfficiency = 0;

    TRestDataSetSignalSelection();
    /// \brief Process read from its YAML section (LoadConfig)
    TRestDataSetSignalSelection(const std::string& instanceName, const YAML::Node& node);

    std::string GetClassName() const override { return "TRestDataSetSignalSelection"; }
    /// \brief Reads the fields; throws without observable or with quantiles out of 0 <= lower < upper <= 1
    void LoadConfig() override;

    /// \brief Keeps the entries of the dataset with the observable in the range (filter)
    void Apply(TRestDataSet& dataSet) const override;
};

#endif
