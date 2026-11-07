#ifndef TRESTDATASETLOGODDS_H
#define TRESTDATASETLOGODDS_H

#include <TH1D.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TRestDataSetProcess.h"

/// \class TRestDataSetLogOddsObservable
/// \brief An observable of a TRestDataSetLogOdds: the range and bins of its probability density function
/// (PDF).
class TRestDataSetLogOddsObservable : public TRestMetadata {
   public:
    /// Observable (column of the dataset)
    std::string fObservable = "";
    /// Range of the PDF
    std::pair<double, double> fRange = {0, 0};
    /// Number of bins of the PDF
    int fNBins = 100;
    /// Result: probability of each bin (normalized to 1 in the range)
    std::vector<double> fPDF = {};

    TRestDataSetLogOddsObservable();
    TRestDataSetLogOddsObservable(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestDataSetLogOddsObservable"; }

    /// \brief Probability of the bin of a value, 0 outside the range
    double GetProbability(double value) const;
    /// \brief The PDF as a histogram
    std::unique_ptr<TH1D> GetHistogram(const std::string& name) const;
};

/// \class TRestDataSetLogOdds
/// \brief Log odds of observables with respect to their PDFs in a reference dataset: log(1 - p) - log(p)
/// of each observable and their sum. See the class documentation in TRestDataSetLogOdds.cxx.
class TRestDataSetLogOdds : public TRestDataSetProcess {
   protected:
    /// \brief Builds the PDF of each observable in the reference: a histogram in its range normalized to 1,
    /// the result pdf of each observable. Throws if an observable has no entries in its range.
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;

   public:
    /// The observables
    std::vector<TRestDataSetLogOddsObservable> fObservables;

    TRestDataSetLogOdds();
    TRestDataSetLogOdds(const std::string& instanceName, const YAML::Node& node);

    std::string GetClassName() const override { return "TRestDataSetLogOdds"; }
    void LoadConfig() override;

    /// \brief Adds the columns GetColumn(observable) = log(1 - p) - log(p), with p the probability of the
    /// bin of the value of the entry in the PDF (1000 if p is 0, -1000 if p is 1), and GetTotalColumn() with
    /// their sum
    void Apply(TRestDataSet& dataSet) const override;
    /// \brief Writes the PDF of each observable as a histogram, pdf_<observable>
    void WriteObjects(TDirectory* directory) const override;

    /// \brief Names of the columns: <name>_<observable> and <name>_total
    std::string GetColumn(const std::string& observable) const { return fName + "_" + observable; }
    std::string GetTotalColumn() const { return fName + "_total"; }
};

#endif
