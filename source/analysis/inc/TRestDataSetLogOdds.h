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
///
/// An element of `observables` of TRestDataSetLogOdds. Keys: `observable`, `range` (required), `nBins` and
/// the result `pdf` (written by the process), e.g.
///
///     observables:
///       - {observable: "trackAnalysis_MaxTrack_XYZ_SigmaZ2", range: [0, 20], nBins: 100}
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
    /// \brief Observable read from its YAML node (LoadConfig)
    TRestDataSetLogOddsObservable(const std::string& name, const YAML::Node& node);
    /// \brief Reads the fields; throws without observable, with an empty range, nBins not positive or a pdf
    /// that has not nBins values
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestDataSetLogOddsObservable"; }

    /// \brief Probability of the bin of a value, 0 outside the range [min, max) or without PDF
    double GetProbability(double value) const;
    /// \brief The PDF as a histogram named `name` (not attached to any directory)
    std::unique_ptr<TH1D> GetHistogram(const std::string& name) const;
};

/// \class TRestDataSetLogOdds
/// \brief Log odds of observables with respect to their PDFs in a reference dataset: log(1 - p) - log(p)
/// of each observable and their sum. See the class documentation in TRestDataSetLogOdds.cxx.
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDataSetLogOdds : public TRestDataSetProcess {
   protected:
    /// \brief Builds the PDF of each observable in the reference: a histogram in its range normalized to 1,
    /// the result pdf of each observable. Throws if an observable has no entries in its range.
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;

   public:
    /// The observables
    std::vector<TRestDataSetLogOddsObservable> fObservables;

    TRestDataSetLogOdds();
    /// \brief Process read from its YAML section (LoadConfig)
    TRestDataSetLogOdds(const std::string& instanceName, const YAML::Node& node);

    std::string GetClassName() const override { return "TRestDataSetLogOdds"; }
    /// \brief Reads the fields; throws without observables
    void LoadConfig() override;

    /// \brief Adds the columns GetColumn(observable) = log(1 - p) - log(p), with p the probability of the
    /// bin of the value of the entry in the PDF (1000 if p is 0, -1000 if p is 1), and GetTotalColumn() with
    /// their sum
    void Apply(TRestDataSet& dataSet) const override;
    /// \brief Writes the PDF of each observable as a histogram, pdf_<observable>
    void WriteObjects(TDirectory* directory) const override;

    /// \brief Name of the column of an observable, <name>_<observable>
    std::string GetColumn(const std::string& observable) const { return fName + "_" + observable; }
    /// \brief Name of the column of the sum, <name>_total
    std::string GetTotalColumn() const { return fName + "_total"; }
};

#endif
