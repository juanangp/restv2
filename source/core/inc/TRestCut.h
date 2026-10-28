#ifndef TRESTCUT_H
#define TRESTCUT_H

#include <TFormula.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TLeaf;
class TTree;

/// \class TRestCut
/// \brief Cut on the observables of the analysis tree: a set of named conditions that must all be
/// fulfilled.
///
/// The conditions are C++ expressions of the branches of the analysis tree (`processName_observable`),
/// e.g. "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225". They are used:
/// * by any process, with its `cuts` parameter (names of TRestCut defined in the manager): the event is
///   rejected after the process if it does not pass them (TRestEventProcess::PassCuts), so a process can
///   cut on its own observables and on those of the previous processes and of the input file;
/// * by TRestSelectionProcess, which only applies them;
/// * by TRestAnalysisPlot, as RDataFrame filters (GetExpression), for the whole data, a panel or a
///   histogram.
///
/// In the pipeline only scalar observables (numbers or booleans) can be used.
///
/// Example:
///
///    fiducial:
///      class: "TRestCut"
///      conditions:
///        originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
///        originY: "trackLineAnalysis_OriginY > 20 && trackLineAnalysis_OriginY < 225"
///
/// Differences with legacy: one key `conditions` (name: expression) instead of the `cut` elements with
/// `value` or `variable` and `condition` (the observables are found in the expression), C++ operators
/// instead of AND/OR, and the cuts can be applied in the pipeline by any process.
class TRestCut : public TRestMetadata {
   private:
    /// Analysis tree the formula reads (Compile), not owned
    TTree* fTree = nullptr;  //!
    /// Expression with the observables replaced by parameters [i]
    std::unique_ptr<TFormula> fFormula;  //!
    /// Leaf of each parameter of the formula
    std::vector<TLeaf*> fLeaves;  //!
    mutable std::vector<double> fValues;  //!

   public:
    /// Conditions, name: expression of the analysis tree branches. All of them must be fulfilled
    std::map<std::string, std::string> fConditions;

    TRestCut();
    TRestCut(const std::string& instanceName, const YAML::Node& node);
    TRestCut(const std::string& fileName, const std::string& sectionName);

    std::string GetClassName() const override { return "TRestCut"; }

    void LoadConfig() override;
    void Initialize() override {}

    /// \brief All the conditions together, "(c1) && (c2) && ...", e.g. for TTree::Draw or
    /// RDataFrame::Filter. Empty without conditions.
    std::string GetExpression() const;

    /// \brief Prepares the evaluation of the conditions on the current values of the branches of `tree`
    /// (e.g. the output analysis tree during the pipeline). Nothing is done if it is already compiled for
    /// this tree. Throws if an observable is not a scalar or the expression is not valid.
    void Compile(TTree* tree);

    /// \brief True if the current values of the branches fulfill all the conditions. Compile first.
    bool Evaluate() const;
};

#endif
