#ifndef TRESTEVENT_H
#define TRESTEVENT_H

#include <TH1.h>
#include <TPad.h>
#include <TString.h>
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
struct TRestEventInfo {
    int runOrigin = 0;
    int subRunOrigin = 0;
    int eventID = 0;
    int subEventID = 0;
    long long timeSeconds = 0;
    int timeNanoSeconds = 0;
    bool ok = true;
    std::string subEventTag="";
};

/// \class TRestEvent
/// \brief Abstract base class for all physics event payloads.
///
/// Derived events are persisted in ROOT trees and share common event metadata
/// (run IDs, timestamps, status, and subevent tag).
class TRestEvent {
   protected:
    /// Logical event class/instance name.
    std::string fName = "";
    std::string* fPtrSubEventTag = nullptr; 
    TRestRun *fRestRun = nullptr;
    mutable TPad* fPad = nullptr;

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

    virtual void InitializeReferences(TRestRun* run);
    void SetRestRun(TRestRun* run);
    /// \brief Draws event representation.
    /// \param option Draw option string.
    /// \return Pointer to generated pad.
    virtual TPad* DrawEvent(const TString& option = "") const = 0;

    /// \brief One DrawEvent option written as name(rootOption)[value], e.g. hist(Cont1,col)[3]; the
    /// parts in brackets are optional (empty rootOption, value 0).
    struct DrawOption {
        std::string name;
        std::string rootOption;
        double value = 0;
    };
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
    void SetSubEventTag(const std::string& tag) {
        fInfo.subEventTag = tag;
    }

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
        tree->SetBranchAddress("runOrigin", &fInfo.runOrigin);
        tree->SetBranchAddress("subRunOrigin", &fInfo.subRunOrigin);
        tree->SetBranchAddress("eventID", &fInfo.eventID);
        tree->SetBranchAddress("subEventID", &fInfo.subEventID);
        tree->SetBranchAddress("timeSeconds", &fInfo.timeSeconds);
        tree->SetBranchAddress("timeNanoSecs", &fInfo.timeNanoSeconds);
        tree->SetBranchAddress("ok", &fInfo.ok);
        fPtrSubEventTag = &fInfo.subEventTag;
        tree->SetBranchAddress("subEventTag", &fPtrSubEventTag);
    }

    /// \brief Refreshes cached views after ROOT read operations.
    virtual void RefreshViews() const {}

    /// \brief Copies generic event content from another event.
    /// \param other Source event pointer.
    virtual void CopyFrom(const TRestEvent* other) {
      this->SetEventInfo(other);
      this->fPtrSubEventTag = &this->fInfo.subEventTag;
    }

    virtual void MoveFrom(TRestEvent* other) {
    if (!other) return;
    this->SetEventInfo(other);
    this->fPtrSubEventTag = &this->fInfo.subEventTag;
   }

    /// \brief Prints event summary.
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

    EventRegistry(const EventRegistry&) = delete;
    EventRegistry& operator=(const EventRegistry&) = delete;

    /// \brief Registers an event creator by type key.
    /// \param type Event type key.
    /// \param creator Event creator function.
    void Register(const std::string& type, Creator creator) { creators.emplace(type, std::move(creator)); }

    /// \brief Creates an event instance by type.
    /// \param type Event type key.
    /// \param instanceName Optional instance name override.
    /// \return Newly created event.
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
