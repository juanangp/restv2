#ifndef TRESTSELECTIONPROCESS_H
#define TRESTSELECTIONPROCESS_H

#include <string>

#include "TRestEventProcess.h"

/// \class TRestSelectionProcess
/// \brief Selects the events that pass its TRestCut (`cuts`), for any event type: the event is copied
/// unchanged. The cuts are on the observables of the previous processes or of the input file.
///
/// \code{.yaml}
/// fiducial:
///   class: TRestCut
///   conditions:
///     originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
/// pipeline:
///   class: TRestEventProcessManager
///   select:
///     class: TRestSelectionProcess
///     event: TRestTrackEvent   # required if it is the first process
///     cuts: ["fiducial"]
/// \endcode
class TRestSelectionProcess : public TRestEventProcess {
   public:
    /// Event type (e.g. TRestTrackEvent). Empty: the output event of the previous process
    std::string fEvent = "";

    TRestSelectionProcess();  ///< Default constructor.
    /// \brief Constructs from a YAML node.
    TRestSelectionProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructs from a section of a YAML file.
    TRestSelectionProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads `event` and the common process fields.
    void LoadConfig() override;
    /// \brief Warns if there are no cuts (all the events are selected).
    void InitProcess() override;
    /// \brief Copies the input event to the output (the selection is done by the cuts).
    /// \return Always true.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;

    std::string GetInputEvent() const override { return fEvent; }   ///< The `event` type.
    std::string GetOutputEvent() const override { return fEvent; }  ///< The `event` type.

    /// \brief Returns "TRestSelectionProcess".
    std::string GetClassName() const override { return "TRestSelectionProcess"; }
};

#endif
