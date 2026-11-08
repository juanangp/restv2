#ifndef TRESTEVENT_H
#define TRESTEVENT_H

#include <TFile.h>
#include <TH1.h>
#include <TPad.h>
#include <TParameter.h>
#include <TTree.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

class TRestRun;
class TPad;

/// \struct TRestEventInfo
/// \brief POD structure with generic event identity and timestamp fields.
///
/// Each field is written as its own branch of the event tree (see TRestEvent::CreateBranches).
struct TRestEventInfo {
    int runOrigin = 0;             ///< Run number the event comes from.
    int subRunOrigin = 0;          ///< Subrun number the event comes from.
    int eventID = 0;               ///< Event identifier.
    int subEventID = 0;            ///< Subevent identifier (several subevents may share an eventID).
    long long timeSeconds = 0;     ///< Whole seconds of the event timestamp (unix time).
    int timeNanoSeconds = 0;       ///< Nanoseconds of the timestamp, in [0, 1e9).
    bool ok = true;                ///< Validity flag (false if a process marked the event as bad).
    std::string subEventTag = "";  ///< Free label of the subevent.
};

/// \class TRestEvent
/// \brief Abstract base class for all physics event payloads.
///
/// Derived events are persisted in ROOT trees and share common event metadata
/// (run IDs, timestamps, status, and subevent tag).
///
/// A derived event must implement GetClassName, Initialize (calling TRestEvent::Initialize) and
/// DrawEvent, and usually extends CreateBranches / SetBranchAddresses with its own branches (calling
/// the base versions first). Event types are created by name through EventRegistry, where each event
/// class registers itself.
class TRestEvent {
   protected:
    /// Logical event class/instance name.
    std::string fName = "";
    /// Pointer to fInfo.subEventTag, used as branch address of the string branch.
    std::string* fPtrSubEventTag = nullptr;
    /// Run the event belongs to (not owned), see SetRestRun.
    TRestRun* fRestRun = nullptr;
    /// Pad created by DrawEvent (owned, deleted in the destructor).
    mutable TPad* fPad = nullptr;
    /// Format version of the input tree (TRestRun), for SetBranchAddresses
    int fInputFormatVersion = 0;

   public:
    /// Generic event metadata block.
    TRestEventInfo fInfo;

    /// \brief Returns the concrete event class name.
    /// \return Class name string.
    virtual std::string GetClassName() const = 0;

    /// \brief Resets event to default state.
    ///
    /// Pure virtual so every event must reset its own payload, but defined (below) to
    /// reset fInfo: overrides must call TRestEvent::Initialize().
    virtual void Initialize() = 0;

    /// \brief Sets run and subrun origin from the run numbers of the given run.
    /// \param run Run providing the run and subrun numbers (must not be null).
    virtual void InitializeReferences(TRestRun* run);
    /// \brief Stores the run the event belongs to (not owned).
    /// \param run Run pointer.
    void SetRestRun(TRestRun* run);
    /// \brief Run the event was read from (SetRestRun), nullptr if none.
    TRestRun* GetRestRun() const { return fRestRun; }
    /// \brief Draws event representation.
    /// \param option Draw option string.
    /// \return Pointer to generated pad.
    virtual TPad* DrawEvent(const std::string& option = "") const = 0;

    /// \brief One DrawEvent option written as name(rootOption)[value], e.g. hist(Cont1,col)[3]; the
    /// parts in brackets are optional (empty rootOption, value 0).
    struct DrawOption {
        std::string name;        ///< Option name (before the parentheses or brackets).
        std::string rootOption;  ///< ROOT draw option given in parentheses.
        double value = 0;        ///< Numeric value given in brackets.
    };
    /// \brief Splits a draw option of the form name(rootOption)[value] into its parts.
    /// \param option Option string, e.g. "hist(Cont1,col)[3]".
    /// \return Parsed option (missing parts empty or 0).
    static DrawOption ParseDrawOption(const std::string& option) {
        const size_t open = option.find('('), close = option.find(')');
        const size_t openValue = option.find('['), closeValue = option.find(']');
        DrawOption parsed;
        parsed.name = option.substr(0, std::min(open, openValue));
        if (open != std::string::npos && close != std::string::npos)
            parsed.rootOption = option.substr(open + 1, close - open - 1);
        if (openValue != std::string::npos && closeValue != std::string::npos)
            parsed.value = std::stod(option.substr(openValue + 1, closeValue - openValue - 1));
        return parsed;
    }

    /// \brief Margins of the event viewers, so the axis titles are not clipped.
    static void SetPadMargins(TVirtualPad* pad) {
        pad->SetBottomMargin(0.13);
        pad->SetLeftMargin(0.15);
    }

    /// \brief Draws a ROOT object with axes (TH1/TH2/TH3, TGraph, TMultiGraph, ...) in the current pad,
    /// which owns it (deleted with the pad), with the REST axis title and label sizes. Histograms are not
    /// attached to any directory and have no stats box. A null title keeps the title of the object.
    template <class T>
    static void DrawOwned(T* object, const std::string& option, const char* xTitle = nullptr,
                          const char* yTitle = nullptr, const char* zTitle = nullptr) {
        if constexpr (std::is_base_of_v<TH1, T>) {
            object->SetDirectory(nullptr);
            object->SetStats(false);
        }
        object->SetBit(TObject::kCanDelete);
        object->Draw(option.c_str());

        // After Draw: graphs create their axes when drawn
        TAxis* xAxis = object->GetXaxis();
        TAxis* yAxis = object->GetYaxis();
        if (!xAxis || !yAxis) return;
        if (xTitle) xAxis->SetTitle(xTitle);
        if (yTitle) yAxis->SetTitle(yTitle);
        if constexpr (std::is_base_of_v<TH1, T>) {
            if (zTitle) object->GetZaxis()->SetTitle(zTitle);
        }
        xAxis->SetTitleSize(1.4 * xAxis->GetTitleSize());
        yAxis->SetTitleSize(1.4 * yAxis->GetTitleSize());
        xAxis->SetLabelSize(1.25 * xAxis->GetLabelSize());
        yAxis->SetLabelSize(1.25 * yAxis->GetLabelSize());
        yAxis->SetTitleOffset(1);
    }

    /// \brief Sets event name.
    /// \param name Event name.
    void SetName(const std::string& name) { fName = name; }

    /// \brief Returns event name.
    /// \return Event name.
    std::string GetName() const { return fName; }

    /// \brief Sets run origin ID.
    /// \param v Run number.
    void SetRunOrigin(int v) { fInfo.runOrigin = v; }

    /// \brief Sets subrun origin ID.
    /// \param v Subrun number.
    void SetSubRunOrigin(int v) { fInfo.subRunOrigin = v; }

    /// \brief Sets event ID.
    /// \param id Event identifier.
    void SetID(int id) { fInfo.eventID = id; }

    /// \brief Sets subevent ID.
    /// \param id Subevent identifier.
    void SetSubID(int id) { fInfo.subEventID = id; }

    /// \brief Sets subevent tag.
    /// \param tag Subevent label.
    void SetSubEventTag(const std::string& tag) { fInfo.subEventTag = tag; }

    /// \brief Sets event validity state.
    /// \param s Validity flag.
    void SetState(bool s) { fInfo.ok = s; }

    /// \brief Sets event validity state.
    /// \param s Validity flag.
    void SetOK(bool s) { fInfo.ok = s; }

    /// \brief Sets event timestamp from floating-point seconds.
    /// \param time Seconds from epoch/reference.
    void SetTime(double time);

    /// \brief Sets event timestamp from integer fields.
    /// \param seconds Whole seconds.
    /// \param nanoseconds Nanoseconds component.
    void SetTime(long seconds, int nanoseconds) {
        fInfo.timeSeconds = seconds;
        fInfo.timeNanoSeconds = nanoseconds;
    }

    /// \brief Copies event-info block from another event.
    /// \param eve Source event pointer.
    void SetEventInfo(const TRestEvent* eve);

    /// \brief Returns event ID.
    /// \return Event ID.
    int GetID() const { return fInfo.eventID; }

    /// \brief Returns subevent ID.
    /// \return Subevent ID.
    int GetSubID() const { return fInfo.subEventID; }

    /// \brief Returns subevent tag.
    /// \return Subevent tag string.
    std::string GetSubEventTag() const { return fInfo.subEventTag; }

    /// \brief Returns run origin.
    /// \return Run number.
    int GetRunOrigin() const { return fInfo.runOrigin; }

    /// \brief Returns subrun origin.
    /// \return Subrun number.
    int GetSubRunOrigin() const { return fInfo.subRunOrigin; }

    /// \brief Returns event timestamp in seconds.
    /// \return Timestamp in seconds.
    double GetTime() const { return fInfo.timeSeconds + fInfo.timeNanoSeconds * 1E-9; }

    /// \brief Returns event validity flag.
    /// \return `true` if event is valid.
    bool isOk() const { return fInfo.ok; }

    /// \brief Returns full event-info structure.
    /// \return Const reference to event info.
    const TRestEventInfo& GetEventInfo() const { return fInfo; }

    /// \brief Creates common ROOT branches for event metadata.
    /// \param tree Target ROOT tree.
    virtual void CreateBranches(TTree* tree) {
        tree->Branch("runOrigin", &fInfo.runOrigin, "runOrigin/I");
        tree->Branch("subRunOrigin", &fInfo.subRunOrigin, "subRunOrigin/I");
        tree->Branch("eventID", &fInfo.eventID, "eventID/I");
        tree->Branch("subEventID", &fInfo.subEventID, "subEventID/I");
        tree->Branch("timeSeconds", &fInfo.timeSeconds, "timeSeconds/L");
        tree->Branch("timeNanoSecs", &fInfo.timeNanoSeconds, "timeNanoSecs/I");
        tree->Branch("ok", &fInfo.ok, "ok/O");
        tree->Branch("subEventTag", &fInfo.subEventTag);
    }

    /// \brief Binds common ROOT branch addresses for event metadata.
    /// \param tree Source ROOT tree.
    virtual void SetBranchAddresses(TTree* tree) {
        BindBranch(tree, "runOrigin", &fInfo.runOrigin);
        BindBranch(tree, "subRunOrigin", &fInfo.subRunOrigin);
        BindBranch(tree, "eventID", &fInfo.eventID);
        BindBranch(tree, "subEventID", &fInfo.subEventID);
        BindBranch(tree, "timeSeconds", &fInfo.timeSeconds);
        BindBranch(tree, "timeNanoSecs", &fInfo.timeNanoSeconds);
        BindBranch(tree, "ok", &fInfo.ok);
        fPtrSubEventTag = &fInfo.subEventTag;
        BindBranch(tree, "subEventTag", &fPtrSubEventTag);
    }

    /// \brief Version of the branch layout of the event class, stored in the UserInfo of its tree
    /// (TParameter<int> "formatVersion", written by TRestRun::RegisterEvent) to read files written with
    /// older layouts. Increase it in the class when its branches change, and convert the older ones in
    /// SetBranchAddresses (GetInputFormatVersion) or raise GetMinimumFormatVersion
    virtual int GetFormatVersion() const { return 1; }

    /// \brief Oldest format version that SetBranchAddresses reads; 0: also the trees without version
    /// (written before 2026-11-02)
    virtual int GetMinimumFormatVersion() const { return 0; }

    /// \brief Format version of the input tree (0 without version), set by TRestRun before
    /// SetBranchAddresses
    int GetInputFormatVersion() const { return fInputFormatVersion; }
    /// \brief Sets the format version of the input tree (called by TRestRun before SetBranchAddresses).
    /// \param version Format version read from the tree UserInfo.
    void SetInputFormatVersion(int version) { fInputFormatVersion = version; }

    /// Name of the TParameter<int> holding the format version in the tree UserInfo.
    static constexpr const char* kFormatVersionKey = "formatVersion";

    /// \brief SetBranchAddress of a branch that must be in the tree, for SetBranchAddresses. ROOT only prints
    /// an error for a missing branch and the events would be read without its data, so it throws: the file
    /// is not compatible with this version of the event class (a new branch without raising
    /// GetFormatVersion, or a file of another layout)
    template <typename T>
    void BindBranch(TTree* tree, const char* name, T* address) const {
        if (!tree->GetBranch(name)) {
            const TFile* file = tree->GetCurrentFile();
            throw std::runtime_error("TRestEvent: the tree " + std::string(tree->GetName()) +
                                     (file ? " of " + std::string(file->GetName()) : std::string()) +
                                     " has no branch " + name + " of " + GetClassName() +
                                     " (format version " + std::to_string(fInputFormatVersion) +
                                     "): not compatible with this REST");
        }
        tree->SetBranchAddress(name, address);
    }

    /// \brief Writes GetFormatVersion in the UserInfo of the tree (once)
    void WriteFormatVersion(TTree* tree) const {
        auto* userInfo = tree->GetUserInfo();
        if (userInfo->FindObject(kFormatVersionKey)) return;
        userInfo->Add(new TParameter<int>(kFormatVersionKey, GetFormatVersion()));
    }

    /// \brief The format version in the UserInfo of the tree, 0 without version
    static int ReadFormatVersion(TTree* tree) {
        const auto* version =
            dynamic_cast<TParameter<int>*>(tree->GetUserInfo()->FindObject(kFormatVersionKey));
        return version ? version->GetVal() : 0;
    }

    /// \brief Refreshes cached views after ROOT read operations (no-op by default).
    virtual void RefreshViews() const {}

    /// \brief Copies generic event content from another event.
    /// \param other Source event pointer.
    virtual void CopyFrom(const TRestEvent* other) {
        this->SetEventInfo(other);
        this->fPtrSubEventTag = &this->fInfo.subEventTag;
    }

    /// \brief Moves the content of another event into this one; the base class copies the event info.
    /// \param other Source event pointer (ignored if null).
    virtual void MoveFrom(TRestEvent* other) {
        if (!other) return;
        this->SetEventInfo(other);
        this->fPtrSubEventTag = &this->fInfo.subEventTag;
    }

    /// \brief Prints event summary (IDs, timestamp, tag and status).
    virtual void PrintEvent() const;

    /// \brief Default constructor.
    TRestEvent() = default;

    /// \brief Virtual destructor.
    virtual ~TRestEvent() {
        if (fPad != nullptr) {
            delete fPad;
            fPad = nullptr;
        }
    }
};

/// fName and the branch pointers are kept: they identify the event, not its content.
inline void TRestEvent::Initialize() { fInfo = TRestEventInfo(); }

/// \class EventRegistry
/// \brief Runtime factory registry for TRestEvent-derived classes.
///
/// Singleton mapping an event type name (e.g. "TRestRawSignalEvent") to a creator function, used by
/// TRestRun and the processes to instantiate events from their names.
class EventRegistry {
   public:
    /// Function type used to instantiate events.
    using Creator = std::function<std::unique_ptr<TRestEvent>()>;

    /// \brief Returns the singleton registry instance.
    /// \return Singleton reference.
    static EventRegistry& Instance() {
        static EventRegistry inst;
        return inst;
    }

    EventRegistry(const EventRegistry&) = delete;  ///< Non-copyable singleton.
    EventRegistry& operator=(const EventRegistry&) = delete;

    /// \brief Registers an event creator by type key (an existing key is kept).
    /// \param type Event type key.
    /// \param creator Event creator function.
    void Register(const std::string& type, Creator creator) { creators.emplace(type, std::move(creator)); }

    /// \brief Creates an event instance by type.
    /// \param type Event type key.
    /// \param instanceName Optional instance name override.
    /// \return Newly created event, named instanceName or the type if empty.
    /// \throws std::runtime_error if the type is not registered.
    std::unique_ptr<TRestEvent> Create(const std::string& type, const std::string& instanceName = "") const {
        auto it = creators.find(type);
        if (it == creators.end()) throw std::runtime_error("EventRegistry: unknown type '" + type + "'");

        auto event = it->second();
        if (!instanceName.empty()) {
            event->SetName(instanceName);
        } else {
            event->SetName(type);
        }
        return event;
    }

    /// \brief Checks whether an event type is registered.
    /// \param type Event type key.
    /// \return `true` if the type is available.
    bool Contains(const std::string& type) const { return creators.count(type) != 0; }

    /// \brief Returns all registered event type keys.
    /// \return Vector of registered type names.
    std::vector<std::string> GetRegisteredTypes() const {
        std::vector<std::string> keys;
        keys.reserve(creators.size());
        for (const auto& [k, v] : creators) keys.push_back(k);
        return keys;
    }

   private:
    EventRegistry() = default;
    std::map<std::string, Creator> creators;
};

#endif
