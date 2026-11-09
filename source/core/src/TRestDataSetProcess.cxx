#include "TRestDataSetProcess.h"

#include <TDirectory.h>
#include <TFile.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>

#include "TRestCut.h"
#include "TRestDataSet.h"
#include "TRestManager.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

static const bool TRestDataSetProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetProcess>("referenceFileName", &TRestDataSetProcess::fReferenceFileName);
    reg.RegisterField<TRestDataSetProcess>("referenceSelection", &TRestDataSetProcess::fReferenceSelection);
    reg.RegisterField<TRestDataSetProcess>("cuts", &TRestDataSetProcess::fCutNames);
    reg.RegisterField<TRestDataSetProcess>("referenceUsed", &TRestDataSetProcess::fReferenceUsed);
    reg.RegisterField<TRestDataSetProcess>("parametersFileName", &TRestDataSetProcess::fParametersFileName);
    return true;
}();

std::string TRestDataSetProcess::AsDouble(ROOT::RDF::RNode& frame, const std::string& column) const {
    const std::string type = frame.GetColumnType(column);
    if (type == "double" || type == "Double_t") return column;
    const std::string name = fName + "_" + column + "_double";
    if (!frame.HasColumn(name)) frame = frame.Define(name, "static_cast<double>(" + column + ")");
    return name;
}

void TRestDataSetProcess::LoadStoredConfig(const YAML::Node& config) {
    SetYAMLNode(TRestYAMLUtils::Detach(config));
    LoadConfig();
}

std::string TRestDataSetProcess::ResolveReference(const TRestDataSet& dataSet) const {
    const std::string context = GetClassName() + " '" + fName + "'";
    if (fReferenceFileName.empty()) return dataSet.fInputFileName;
    if (!fReferenceSelection.empty() && fReferenceSelection != "closest") {
        throw std::runtime_error(context + ": referenceSelection '" + fReferenceSelection +
                                 "' is not valid (\"closest\" or empty)");
    }
    std::vector<std::string> files;
    for (const auto& file : TRestTools::GetFilesMatchingPattern(fReferenceFileName))
        files.push_back(TRestTools::GetFullPath(file));
    if (files.empty()) throw std::runtime_error(context + ": no file matches " + fReferenceFileName);
    if (files.size() == 1) return files.front();
    if (fReferenceSelection.empty()) {
        throw std::runtime_error(context + ": " + std::to_string(files.size()) + " files match " +
                                 fReferenceFileName + " (give one, or referenceSelection: closest)");
    }

    // The closest in time: the smallest gap between the times of the datasets, then the closest middles
    const double start = dataSet.GetStartTime(), end = dataSet.GetEndTime();
    std::string closest;
    std::pair<double, double> best = {std::numeric_limits<double>::infinity(), 0};
    for (const auto& fileName : files) {
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
        const auto runs = file && !file->IsZombie() ? TRestDataSet::ReadRuns(file.get()) : std::nullopt;
        if (!runs || runs->empty()) throw std::runtime_error(context + ": " + fileName + " is not a dataset");
        double referenceStart = runs->front().run->GetStartTime(),
               referenceEnd = runs->front().run->GetEndTime();
        for (const auto& run : *runs) {
            referenceStart = std::min(referenceStart, run.run->GetStartTime());
            referenceEnd = std::max(referenceEnd, run.run->GetEndTime());
        }
        const double gap = std::max({0., referenceStart - end, start - referenceEnd});
        const double middles = std::abs((referenceStart + referenceEnd) / 2 - (start + end) / 2);
        if (std::make_pair(gap, middles) < best) {
            best = {gap, middles};
            closest = fileName;
        }
    }
    RESTInfo << context << ": reference " << closest << ", the closest in time of " << files.size()
             << " (gap " << best.first << " s)" << RESTendl;
    return closest;
}

std::string TRestDataSetProcess::GetCutExpression(const std::vector<std::string>& cutNames) const {
    std::string expression;
    for (const auto& name : cutNames) {
        const TRestCut* cut = nullptr;
        if (fManager) {
            for (const auto* c : fManager->GetMetadataOfType<TRestCut>()) {
                if (c->GetName() == name) cut = c;
            }
        }
        if (!cut) {
            throw std::runtime_error(GetClassName() + " '" + fName + "': TRestCut '" + name +
                                     "' not found in the manager");
        }
        const std::string condition = cut->GetExpression();
        if (condition.empty()) continue;
        if (!expression.empty()) expression += " && ";
        expression += "(" + condition + ")";
    }
    return expression;
}

void TRestDataSetProcess::ObtainParameters(const TRestDataSet& dataSet, const TRestManager* manager) {
    fManager = manager;
    // The cuts applied to the reference
    const std::string cutExpression = GetCutExpression(fCutNames);

    // A dataset generated in memory (not in a file yet) is its own reference: computed, it has no processes;
    // the reference is the file it will be written to
    const std::string referenceFile = ResolveReference(dataSet);
    const bool inMemory = referenceFile.empty();
    fReferenceUsed = inMemory ? dataSet.fOutputFileName : referenceFile;

    // The reference processed by this process: its parameters, not computed
    std::unique_ptr<TFile> file;
    YAML::Node stored;
    if (!inMemory) {
        file.reset(TFile::Open(referenceFile.c_str(), "READ"));
        if (!file || file->IsZombie()) {
            throw std::runtime_error(GetClassName() + " '" + fName + "': cannot open " + referenceFile);
        }
        stored = TRestYAMLUtils::ReadMetadataStore(file.get())[fName];
    }
    if (stored && stored.IsMap() && stored["class"] && stored["class"].as<std::string>() == GetClassName()) {
        RESTWarning << GetClassName() << " '" << fName << "': parameters read from " << referenceFile
                    << " (processed by it), not computed" << RESTendl;
        const std::string reference = fReferenceFileName;
        const std::string selection = fReferenceSelection;
        const std::vector<std::string> cuts = fCutNames;
        const std::string verbose = TRestLogManager::GetStringFromVerbose(fVerboseLevel);
        YAML::Node config = TRestYAMLUtils::Detach(stored);
        config["verbose"] = verbose;
        LoadStoredConfig(config);
        ReadObjects(GetStoreDirectory(file.get(), GetClassName(), fName));
        // This configuration, with the stored results
        fReferenceFileName = reference;
        fReferenceSelection = selection;
        fReferenceUsed = referenceFile;
        fCutNames = cuts;
        UpdateYAMLFromParams<TRestDataSetProcess>(fNode);
        return;
    }
    file.reset();

    RESTInfo << GetClassName() << " '" << fName << "': computing the parameters on " << fReferenceUsed
             << RESTendl;
    if (fReferenceFileName.empty()) {
        ROOT::RDF::RNode frame = dataSet.GetDataFrame();
        if (!cutExpression.empty()) frame = frame.Filter(cutExpression);
        ComputeParameters(dataSet, frame);
    } else {
        const TRestDataSet reference(referenceFile);
        ROOT::RDF::RNode frame = reference.GetDataFrame();
        if (!cutExpression.empty()) frame = frame.Filter(cutExpression);
        ComputeParameters(reference, frame);
    }
    fParametersFileName = fReferenceUsed;
    UpdateYAMLFromParams<TRestDataSetProcess>(fNode);
}

void TRestDataSetProcess::ReadFromFile(const std::string& fileName, const std::string& name) {
    std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
    if (!file || file->IsZombie()) {
        throw std::runtime_error(GetClassName() + ": cannot open " + fileName);
    }
    const YAML::Node store = TRestYAMLUtils::ReadMetadataStore(file.get());
    std::string instance = name;
    if (instance.empty()) instance = TRestYAMLUtils::GetMetadataClass(store, GetClassName()).first;
    const YAML::Node config = store[instance];
    if (instance.empty() || !config || !config.IsMap() || !config["class"] ||
        config["class"].as<std::string>() != GetClassName()) {
        throw std::runtime_error(GetClassName() + ": no " + GetClassName() + " '" + name + "' in " +
                                 fileName);
    }
    SetName(instance);
    LoadStoredConfig(config);
    ReadObjects(GetStoreDirectory(file.get(), GetClassName(), instance));
}

TDirectory* TRestDataSetProcess::GetStoreDirectory(TFile* file, const std::string& className,
                                                   const std::string& name) {
    if (!file) return nullptr;
    return file->GetDirectory((kStoreDirectory + "/" + className + "/" + name).c_str());
}

void TRestDataSetProcess::Save(TFile* file) {
    WriteMetadata(file);
    TDirectory* store = file->mkdir(kStoreDirectory.c_str(), "", true);
    TDirectory* classDirectory = store ? store->mkdir(GetClassName().c_str(), "", true) : nullptr;
    if (classDirectory && classDirectory->GetDirectory(fName.c_str())) classDirectory->rmdir(fName.c_str());
    TDirectory* directory = classDirectory ? classDirectory->mkdir(fName.c_str()) : nullptr;
    if (!directory) {
        throw std::runtime_error(GetClassName() + " '" + fName + "': cannot create its folder in " +
                                 file->GetName());
    }
    directory->cd();
    WriteObjects(directory);
    file->cd();
}

void TRestDataSetProcess::UpdateFile(const std::string& fileName) {
    std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "UPDATE"));
    if (!file || file->IsZombie()) {
        throw std::runtime_error(GetClassName() + " '" + fName + "': cannot open " + fileName);
    }
    Save(file.get());
    file->Close();
    RESTInfo << GetClassName() << " '" << fName << "' updated in " << fileName
             << " (the columns of the dataset are not recomputed)" << RESTendl;
}
