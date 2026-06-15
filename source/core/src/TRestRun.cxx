#include "TRestRun.h"

#include <stdexcept>

#include "TRestTools.h"

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataRegistry::Instance().Register(
        "TRestRun",
        [](const std::string& name, const YAML::Node& params) {
            return std::make_unique<TRestRun>(name, params);
        });
    return true;
}();
}  // namespace

// ---------------------------------------------------------------------------
TRestRun::TRestRun(const std::string& sectionName, const YAML::Node& node)
    : TRestMetadata(sectionName, node) {
    LoadConfig();
}

TRestRun::TRestRun(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    YAML::Node cfg = ResolveAllRefs(raw);
    fNode          = cfg[sectionName]["params"];
    LoadConfig();
}

// ---------------------------------------------------------------------------
void TRestRun::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRun::LoadConfig – node is missing or null" << RESTendl;
        return;
    }

    if (fNode["runNumber"]) {
        if (ReadYAMLParam<std::string>(fNode["runNumber"]) == "auto") {
            // TODO: implement automatic run numbering
        } else {
            fRunNumber = ReadYAMLParam<int>(fNode["runNumber"]);
        }
    }

    fParentRunNumber = ReadYAMLParamOrDefault<int>(fNode,         "parentRunNumber", fParentRunNumber);
    fRunType         = ReadYAMLParamOrDefault<std::string>(fNode, "runType",         fRunType);
    fRunUser         = ReadYAMLParamOrDefault<std::string>(fNode, "runUser",         fRunUser);
    fRunTag          = ReadYAMLParamOrDefault<std::string>(fNode, "runTag",          fRunTag);
    fRunDescription  = ReadYAMLParamOrDefault<std::string>(fNode, "runDescription",  fRunDescription);
    fExperimentName  = ReadYAMLParamOrDefault<std::string>(fNode, "experimentName",  fExperimentName);
    fInputFileName   = ReadYAMLParamOrDefault<std::string>(fNode, "inputFileName",   fInputFileName);
    fOutputFileName  = ReadYAMLParamOrDefault<std::string>(fNode, "outputFileName",  fOutputFileName);
    fEntriesSaved    = ReadYAMLParamOrDefault<int>(fNode,         "entriesSaved",    fEntriesSaved);

    TRestMetadata::ReadYAMLVerbose(fNode);
}

// ---------------------------------------------------------------------------
// AOD file management
// ---------------------------------------------------------------------------
void TRestRun::OpenOutputFile() {
    fOutputFile = std::make_unique<TFile>(fOutputFileName.c_str(), "RECREATE");
    if (!fOutputFile || fOutputFile->IsZombie())
        throw std::runtime_error("TRestRun: cannot open output file: " + fOutputFileName);

    fOutputFile->cd();
    fEventTree = new TTree("EventTree", "REST AOD Event Tree");
    fEventTree->SetAutoSave(0);
    RESTInfo << "Opened output file: " << fOutputFileName << RESTendl;
}

void TRestRun::FillEvent() {
    if (!fEventTree)
        throw std::runtime_error("TRestRun: FillEvent() called before OpenOutputFile()");
    fEventTree->Fill();
    ++fEntriesSaved;
}

void TRestRun::CloseOutputFile() {
    if (!fOutputFile) return;
    fOutputFile->cd();
    if (fEventTree) fEventTree->Write("", TObject::kOverwrite);
    fOutputFile->Close();
    fOutputFile.reset();
    fEventTree = nullptr;
    RESTInfo << "Closed output file. Entries saved: " << fEntriesSaved << RESTendl;
}

// ---------------------------------------------------------------------------
void TRestRun::PrintMetadata() {
    RESTMetadata << "=== TRestRun ===" << RESTendl;
    RESTMetadata << fNode << RESTendl;
}
