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

// ============================================================
//  TRestEvent
//  Abstract base for all physics events.
//  Derived instances are stored in ROOT files via TTree/AOD.
// ============================================================
class TRestEvent {
   protected:
    struct EventInfo {
        int runOrigin = 0;
        int subRunOrigin = 0;
        int eventID = 0;
        int subEventID = 0;
        long long timeSeconds = 0;
        int timeNanoSeconds = 0;
        bool ok = true;
    } fInfo;
    std::string fName = "";
    std::string fSubEventTag = "";

   public:
    virtual std::string GetClassName() const = 0;
    virtual void        Initialize()         = 0;
    virtual TPad*       DrawEvent(const TString& option = "") const = 0;

    // Setters
    void SetName(const std::string& name)   { fName = name; }
    std::string GetName() const             { return fName; }
    void SetRunOrigin(int v)                { fInfo.runOrigin    = v; }
    void SetSubRunOrigin(int v)             { fInfo.subRunOrigin = v; }
    void SetID(int id)                      { fInfo.eventID      = id; }
    void SetSubID(int id)                   { fInfo.subEventID   = id; }
    void SetSubEventTag(const TString& tag) { fSubEventTag  = tag.Data(); }
    void SetState(bool s)                   { fInfo.ok           = s; }
    void SetOK(bool s)                      { fInfo.ok           = s; }
    void SetTime(double time);
    void SetTime(long seconds, int nanoseconds){fInfo.timeSeconds = seconds; fInfo.timeNanoSeconds = nanoseconds;}
    void SetEventInfo(const TRestEvent* eve);

    // Getters
    int         GetID()           const { return fInfo.eventID; }
    int         GetSubID()        const { return fInfo.subEventID; }
    std::string GetSubEventTag()  const { return fSubEventTag; }
    int         GetRunOrigin()    const { return fInfo.runOrigin; }
    int         GetSubRunOrigin() const { return fInfo.subRunOrigin; }
    double      GetTime()         const { return fInfo.timeSeconds + fInfo.timeNanoSeconds*1E-9; }
    bool        isOk()            const { return fInfo.ok; }

    virtual void CreateBranches(TTree* tree) {
      tree->Branch("runOrigin",    &fInfo.runOrigin,    "runOrigin/I");
      tree->Branch("subRunOrigin", &fInfo.subRunOrigin, "subRunOrigin/I");
      tree->Branch("eventID",      &fInfo.eventID,      "eventID/I");
      tree->Branch("timeSeconds",  &fInfo.timeSeconds,  "timeSeconds/L");
      tree->Branch("timeNanoSecs", &fInfo.timeNanoSeconds, "timeNanoSecs/I");
      tree->Branch("ok",           &fInfo.ok,           "ok/O");
      tree->Branch("subEventTag",  &fSubEventTag);
    }

    virtual void SetBranchAddresses(TTree* tree) {
        tree->SetBranchAddress("runOrigin", &fInfo.runOrigin);
        tree->SetBranchAddress("subRunOrigin", &fInfo.subRunOrigin);
        tree->SetBranchAddress("eventID", &fInfo.eventID);
        tree->SetBranchAddress("timeSeconds", &fInfo.timeSeconds);
        tree->SetBranchAddress("timeNanoSecs", &fInfo.timeNanoSeconds);
        tree->SetBranchAddress("ok", &fInfo.ok);
    }

    virtual void RefreshViews() const { }

    virtual void CopyFrom(const TRestEvent* other) {
      this->SetEventInfo(other);
    }



    virtual void PrintEvent() const;

    TRestEvent()          = default;
    virtual ~TRestEvent() = default;
};

class EventRegistry {
   public:
    using Creator = std::function<std::unique_ptr<TRestEvent>()>;

    static EventRegistry& Instance() {
        static EventRegistry inst;
        return inst;
    }

    EventRegistry(const EventRegistry&)            = delete;
    EventRegistry& operator=(const EventRegistry&) = delete;

    void Register(const std::string& type, Creator creator) {
        creators.emplace(type, std::move(creator));
    }

    std::unique_ptr<TRestEvent> Create(const std::string& type, const std::string& instanceName = "") const {
        auto it = creators.find(type);
        if (it == creators.end())
            throw std::runtime_error("EventRegistry: unknown type '" + type + "'");
        
        auto event = it->second();
        if (!instanceName.empty()) {
            event->SetName(instanceName);
        } else {
            event->SetName(type); // Fallback to class name
        }
        return event;
    }

    bool Contains(const std::string& type) const { return creators.count(type) != 0; }

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
