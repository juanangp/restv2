#pragma once

#include <TPad.h>
#include <TString.h>
#include <TTree.h>

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
};

/// \class TRestEvent
/// \brief Abstract base class for all physics event payloads.
///
/// Derived events are persisted in ROOT trees and share common event metadata
/// (run IDs, timestamps, status, and subevent tag).
class TRestEvent {
   protected:
    /// Generic event metadata block.
    TRestEventInfo fInfo;

    /// Logical event class/instance name.
    std::string fName = "";

    /// Optional subevent categorization tag.
    TString fSubEventTag = "";

    /// Persistent pointer required by ROOT I/O for TString branch handling.
    TString* fSubEventTagPtr = &fSubEventTag;

   public:
    /// \brief Returns the concrete event class name.
    /// \return Class name string.
    virtual std::string GetClassName() const = 0;

    /// \brief Resets event to default state.
    virtual void Initialize() = 0;

    /// \brief Draws event representation.
    /// \param option Draw option string.
    /// \return Pointer to generated pad.
    virtual TPad* DrawEvent(const TString& option = "") const = 0;

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
    void SetSubEventTag(const TString& tag) { fSubEventTag = tag; }

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
    std::string GetSubEventTag() const { return fSubEventTag.Data(); }

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
        tree->Branch("runOrigin",    &fInfo.runOrigin,       "runOrigin/I");
        tree->Branch("subRunOrigin", &fInfo.subRunOrigin,    "subRunOrigin/I");
        tree->Branch("eventID",      &fInfo.eventID,         "eventID/I");
        tree->Branch("subEventID",   &fInfo.subEventID,      "subEventID/I");
        tree->Branch("timeSeconds",  &fInfo.timeSeconds,     "timeSeconds/L");
        tree->Branch("timeNanoSecs", &fInfo.timeNanoSeconds, "timeNanoSecs/I");
        tree->Branch("ok",           &fInfo.ok,              "ok/O");
        tree->Branch("subEventTag",  &fSubEventTagPtr);
    }

    /// \brief Binds common ROOT branch addresses for event metadata.
    /// \param tree Source ROOT tree.
    virtual void SetBranchAddresses(TTree* tree) {
        tree->SetBranchAddress("runOrigin",    &fInfo.runOrigin);
        tree->SetBranchAddress("subRunOrigin", &fInfo.subRunOrigin);
        tree->SetBranchAddress("eventID",      &fInfo.eventID);
        tree->SetBranchAddress("subEventID",   &fInfo.subEventID);
        tree->SetBranchAddress("timeSeconds",  &fInfo.timeSeconds);
        tree->SetBranchAddress("timeNanoSecs", &fInfo.timeNanoSeconds);
        tree->SetBranchAddress("ok",           &fInfo.ok);
        tree->SetBranchAddress("subEventTag",  &fSubEventTagPtr);
    }

    /// \brief Refreshes cached views after ROOT read operations.
    virtual void RefreshViews() const {
        const_cast<TRestEvent*>(this)->fSubEventTag = fSubEventTagPtr ? *fSubEventTagPtr : "";
    }

    /// \brief Copies generic event content from another event.
    /// \param other Source event pointer.
    virtual void CopyFrom(const TRestEvent* other) { this->SetEventInfo(other); }

    /// \brief Prints event summary.
    virtual void PrintEvent() const;

    /// \brief Default constructor.
    TRestEvent() = default;

    /// \brief Virtual destructor.
    virtual ~TRestEvent() = default;
};

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
