#ifndef TRESTDATASETCOMBINE_H
#define TRESTDATASETCOMBINE_H

#include <string>
#include <vector>

#include "TRestDataSetProcess.h"

/// \class TRestDataSetCombine
/// \brief Adds to a dataset the columns of another dataset of the same events (e.g. the analysis of the
/// vetoes, processed apart, to the dataset of the Micromegas), matching the entries by run (runNumber and
/// subRunNumber), eventID and subEventID. See the class documentation in TRestDataSetCombine.cxx.
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDataSetCombine : public TRestDataSetProcess {
   protected:
    /// \brief Nothing to compute (the columns are read from combineFileName in Apply).
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;

   public:
    /// `combineFileName`: the dataset whose columns are added
    std::string fCombineFileName = "";
    /// `observables`: patterns (* and ?) of its columns added; empty: all (but eventID, subEventID,
    /// timeSeconds and runIndex)
    std::vector<std::string> fObservables = {};
    /// `prefix`: added to the names of the columns (needed if a column already exists in the dataset)
    std::string fPrefix = "";
    /// `keepUnmatched`: keep the entries without match in combineFileName (with default values and
    /// `<name>_matched` 0); false: only the matched entries
    bool fKeepUnmatched = true;

    TRestDataSetCombine();
    /// \brief Process read from its YAML section (LoadConfig)
    TRestDataSetCombine(const std::string& instanceName, const YAML::Node& node);

    std::string GetClassName() const override { return "TRestDataSetCombine"; }
    /// \brief Reads the fields; throws without combineFileName
    void LoadConfig() override;
    /// \brief Adds the columns of combineFileName and `<name>_matched` to the dataset (and keeps only the
    /// matched entries without keepUnmatched). Throws if combineFileName cannot be opened, a column has a
    /// type that cannot be combined, a column already exists or an event is twice in combineFileName.
    void Apply(TRestDataSet& dataSet) const override;
};

#endif
