#include "TRestDataSetTMVA.h"

#include <TCanvas.h>
#include <TCut.h>
#include <TDirectory.h>
#include <TLegend.h>
#include <TMVA/Config.h>
#include <TMVA/DataLoader.h>
#include <TMVA/Factory.h>
#include <TMVA/MethodBDT.h>
#include <TMVA/MethodBase.h>
#include <TMVA/Reader.h>
#include <TMVA/Types.h>
#include <TMemFile.h>
#include <TROOT.h>
#include <TTree.h>
#include <unistd.h>

#include <ROOT/RVec.hxx>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "TRestDataSet.h"
#include "TRestTools.h"

/// \class TRestDataSetTMVA
///
/// Multivariate classifier of ROOT TMVA, a process of TRestDataSetProcessManager. A method of TMVA (a
/// boosted decision tree, BDT, by default) is trained to separate a signal, the reference (referenceFileName
/// with the cuts, see TRestDataSetProcess), from a background (backgroundFileName with backgroundCuts), with
/// the `observables` as input variables; the column `<name>_score` with its score is added to the dataset
/// processed. For a BDT, the higher the score, the more signal-like the entry.
///
/// The events of both samples are split in training and test samples (`splitOptions`); the method is
/// trained with the first and evaluated with the second. The weights of the trained method (TMVA XML) are
/// stored in the configuration (`weights`): a dataset processed by this process can be the reference of
/// another, which is then classified with the same method, not trained again.
///
/// ### Parameters
/// * observables: columns of the datasets, the input variables of the method.
/// * backgroundFileName: background dataset (a pattern must match a single dataset).
/// * backgroundCuts: names of the TRestCut of the manager applied to the background.
/// * method: method of TMVA (BDT by default; Likelihood, Fisher, MLP...).
/// * options: options of the method (TMVA option string, e.g. "NTrees=800:MaxDepth=3" for a BDT).
/// * splitOptions: options of TMVA::DataLoader::PrepareTrainingAndTestTree
///   ("SplitMode=Random:NormMode=NumEvents:!V": half of the events of each sample for training).
/// * referenceFileName, cuts: the signal, see TRestDataSetProcess.
///
/// ### Results
/// * weights: the trained method (TMVA XML).
/// * rocIntegral: area under the ROC curve of the test sample.
/// * separation: separation of the score between signal and background in the test sample,
///   <S²> = 1/2 sum (s - b)² / (s + b) over the bins of the normalized distributions (0 for identical
///   distributions, 1 for disjoint ones).
/// * ksSignal, ksBackground: Kolmogorov test of the score between the training and test samples, of the
///   signal and the background (small values: overtraining).
/// * variableSeparation: separation of each observable between signal and background (whole samples), in
///   the order of `observables`, independent of the method.
/// * variableImportance: importance of each observable in a BDT (MethodBDT::GetVariableImportance), in the
///   order of `observables`; empty for the other methods (their ranking is in the log of TMVA, verbose
///   debug).
/// * referenceUsed, parametersFileName: see TRestDataSetProcess.
///
/// Objects (folder RESTDataSetStore/TRestDataSetTMVA/<name> of the output dataset): the ROC curve of the
/// test sample `roc` (background rejection versus signal efficiency), the score distributions of the test
/// sample `score_signal` and `score_background`, and of the training sample `score_signal_train` and
/// `score_background_train` (100 bins).
///
/// DrawResults draws the ROC curve, the score distributions (overtraining check) and the separation and
/// importance of the observables; with the macro REST_ViewTMVA of restRoot for a dataset processed by it:
///
///    restRoot
///    [0] REST_ViewTMVA("Dataset_Background_TMVA.root", "bdt")
///
/// The files of TMVA (TMVA.root, weights/) are not kept. The score is computed with a TMVA::Reader per
/// thread of the data frame.
///
/// Example (in a TRestDataSetProcessManager, the cuts defined in the manager):
///
///    bdt:
///      class: "TRestDataSetTMVA"
///      referenceFileName: "Dataset_Calibration_Calibrated.root"
///      cuts: ["energy", "tracks"]
///      backgroundFileName: "Dataset_Background_Calibrated.root"
///      backgroundCuts: ["tracks"]
///      method: "BDT"
///      options: "NTrees=800:MinNodeSize=2.5%:MaxDepth=3:BoostType=AdaBoost:AdaBoostBeta=0.5:nCuts=20"
///      observables: ["tckAna_MaxTrack_XYZ_SigmaZ2", "tckAna_MaxTrackEnergyRatio"]
///
/// Differences with the TRestDataSetTMVA and TRestDataSetTMVAClassification of the framework pull request
/// #448 (not merged): a single process of a dataset pipeline, training (ComputeParameters) and
/// classification (Apply); a method per process (several processes to compare methods); the weights stored
/// in the dataset instead of a weights file; the cuts are TRestCut of the manager by name.

static const bool TRestDataSetTMVA_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetTMVA>("observables", &TRestDataSetTMVA::fObservables);
    reg.RegisterField<TRestDataSetTMVA>("backgroundFileName", &TRestDataSetTMVA::fBackgroundFileName);
    reg.RegisterField<TRestDataSetTMVA>("backgroundCuts", &TRestDataSetTMVA::fBackgroundCutNames);
    reg.RegisterField<TRestDataSetTMVA>("method", &TRestDataSetTMVA::fMethod);
    reg.RegisterField<TRestDataSetTMVA>("options", &TRestDataSetTMVA::fOptions);
    reg.RegisterField<TRestDataSetTMVA>("splitOptions", &TRestDataSetTMVA::fSplitOptions);
    reg.RegisterField<TRestDataSetTMVA>("weights", &TRestDataSetTMVA::fWeights);
    reg.RegisterField<TRestDataSetTMVA>("rocIntegral", &TRestDataSetTMVA::fROCIntegral);
    reg.RegisterField<TRestDataSetTMVA>("separation", &TRestDataSetTMVA::fSeparation);
    reg.RegisterField<TRestDataSetTMVA>("ksSignal", &TRestDataSetTMVA::fKSSignal);
    reg.RegisterField<TRestDataSetTMVA>("ksBackground", &TRestDataSetTMVA::fKSBackground);
    reg.RegisterField<TRestDataSetTMVA>("variableSeparation", &TRestDataSetTMVA::fVariableSeparation);
    reg.RegisterField<TRestDataSetTMVA>("variableImportance", &TRestDataSetTMVA::fVariableImportance);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetTMVA", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetTMVA>(instanceName, params);
        });
    return true;
}();

/// Number of bins of the distributions (scores and observables)
constexpr int kBins = 100;
/// Names of the score distributions: signal and background, test and training samples
const std::vector<std::string> kScoreNames = {"score_signal", "score_background", "score_signal_train",
                                              "score_background_train"};

/// Separation <S²> = 1/2 sum (s - b)² / (s + b) of two distributions (same binning), normalized to 1
double Separation(const TH1D& signal, const TH1D& background) {
    const double integralS = signal.Integral(), integralB = background.Integral();
    if (integralS <= 0 || integralB <= 0) return 0;
    double separation = 0;
    for (int bin = 1; bin <= signal.GetNbinsX(); bin++) {
        const double s = signal.GetBinContent(bin) / integralS, b = background.GetBinContent(bin) / integralB;
        if (s + b > 0) separation += (s - b) * (s - b) / (s + b);
    }
    return separation / 2;
}

/// Range [min, max] of the values of two samples, widened so that the maximum is inside the last bin
std::pair<double, double> Range(const std::vector<double>& a, const std::vector<double>& b) {
    double min = std::numeric_limits<double>::max(), max = std::numeric_limits<double>::lowest();
    for (const auto* values : {&a, &b}) {
        for (double value : *values) {
            min = std::min(min, value);
            max = std::max(max, value);
        }
    }
    if (min > max) return {0, 1};
    const double margin = max > min ? (max - min) * 1e-6 : 0.5;
    return {min, max + margin};
}

/// Histogram (not attached to any directory) of values in a range
std::unique_ptr<TH1D> MakeHistogram(const std::string& name, const std::string& title,
                                    const std::vector<double>& values, std::pair<double, double> range) {
    auto histogram = std::make_unique<TH1D>(name.c_str(), title.c_str(), kBins, range.first, range.second);
    histogram->SetDirectory(nullptr);
    for (double value : values) histogram->Fill(value);
    return histogram;
}

/// Tree in memory (not attached to any directory) with a double branch per observable
std::unique_ptr<TTree> MakeTree(const std::string& name, const std::vector<std::string>& observables,
                                const std::vector<std::vector<double>>& values) {
    TDirectory::TContext context(nullptr);
    auto tree = std::make_unique<TTree>(name.c_str(), name.c_str());
    std::vector<double> row(observables.size());
    for (size_t i = 0; i < observables.size(); i++)
        tree->Branch(observables[i].c_str(), &row[i], (observables[i] + "/D").c_str());
    const size_t entries = values.empty() ? 0 : values.front().size();
    for (size_t entry = 0; entry < entries; entry++) {
        for (size_t i = 0; i < observables.size(); i++) row[i] = values[i][entry];
        tree->Fill();
    }
    tree->ResetBranchAddresses();
    return tree;
}

/// Values of the observables in a data frame (a vector per observable)
std::vector<std::vector<double>> TakeValues(
    std::vector<ROOT::RDF::RResultPtr<std::vector<double>>>& results) {
    std::vector<std::vector<double>> values;
    for (auto& result : results) values.push_back(*result);
    return values;
}

/// Scores of the training or test tree of TMVA: those of the signal class and of the others
std::pair<std::vector<double>, std::vector<double>> ReadScores(TTree* tree, const std::string& method,
                                                               int signalClass) {
    std::pair<std::vector<double>, std::vector<double>> scores;
    if (!tree) return scores;
    Int_t classID = 0;
    Float_t score = 0;
    tree->SetBranchAddress("classID", &classID);
    tree->SetBranchAddress(method.c_str(), &score);
    for (Long64_t entry = 0; entry < tree->GetEntries(); entry++) {
        tree->GetEntry(entry);
        (classID == signalClass ? scores.first : scores.second).push_back(score);
    }
    tree->ResetBranchAddresses();
    return scores;
}

/// Temporary directory of the weight files of TMVA (TMVA::gConfig), removed with its contents and the
/// previous directory restored at the end
class WeightsDirectory {
    TString fPrevious;
    std::string fPath;

   public:
    WeightsDirectory() : fPrevious(TMVA::gConfig().GetIONames().fWeightFileDirPrefix) {
        std::string pattern = (std::filesystem::temp_directory_path() / "restTMVA_XXXXXX").string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("TRestDataSetTMVA: no temporary directory");
        fPath = pattern;
        TMVA::gConfig().GetIONames().fWeightFileDirPrefix = fPath.c_str();
    }
    ~WeightsDirectory() {
        TMVA::gConfig().GetIONames().fWeightFileDirPrefix = fPrevious;
        std::error_code error;
        std::filesystem::remove_all(fPath, error);
    }
};
}  // namespace

TRestDataSetTMVA::TRestDataSetTMVA() : TRestDataSetProcess() { fName = "TRestDataSetTMVA"; }

TRestDataSetTMVA::TRestDataSetTMVA(const std::string& instanceName, const YAML::Node& node)
    : TRestDataSetProcess(instanceName, node) {
    LoadConfig();
}

TRestDataSetTMVA::~TRestDataSetTMVA() = default;

void TRestDataSetTMVA::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetTMVA>(fNode);
    const std::string context = "TRestDataSetTMVA '" + fName + "'";
    if (fObservables.empty()) throw std::runtime_error(context + ": no observables");
    if (fBackgroundFileName.empty()) throw std::runtime_error(context + ": no backgroundFileName");
    try {
        TMVA::Types::Instance().GetMethodType(fMethod.c_str());
    } catch (const std::exception&) {
        throw std::runtime_error(context + ": unknown method '" + fMethod + "' of TMVA");
    }
    UpdateYAMLFromParams<TRestDataSetTMVA>(fNode);
}

void TRestDataSetTMVA::ComputeParameters(const TRestDataSet&, ROOT::RDF::RNode frame) {
    const std::string context = "TRestDataSetTMVA '" + fName + "'";

    // The background dataset, with its cuts
    const auto files = TRestTools::GetFilesMatchingPattern(fBackgroundFileName);
    if (files.size() != 1) {
        throw std::runtime_error(context + ": " + std::to_string(files.size()) + " files match " +
                                 fBackgroundFileName + " (one is needed)");
    }
    const TRestDataSet background(TRestTools::GetFullPath(files.front()));
    ROOT::RDF::RNode backgroundFrame = background.GetDataFrame();
    const std::string backgroundCut = GetCutExpression(fBackgroundCutNames);
    if (!backgroundCut.empty()) backgroundFrame = backgroundFrame.Filter(backgroundCut);

    // The values of the observables, all booked before the event loops
    std::vector<ROOT::RDF::RResultPtr<std::vector<double>>> signalResults, backgroundResults;
    for (const auto& observable : fObservables) {
        signalResults.push_back(frame.Take<double>(AsDouble(frame, observable)));
        backgroundResults.push_back(backgroundFrame.Take<double>(AsDouble(backgroundFrame, observable)));
    }
    const auto signalValues = TakeValues(signalResults), backgroundValues = TakeValues(backgroundResults);
    const size_t nSignal = signalValues.front().size(), nBackground = backgroundValues.front().size();
    if (nSignal == 0 || nBackground == 0) {
        throw std::runtime_error(context + ": no entries in the signal (" + std::to_string(nSignal) +
                                 ") or the background (" + std::to_string(nBackground) + ")");
    }
    RESTInfo << context << ": training " << fMethod << " with " << nSignal << " signal and " << nBackground
             << " background entries" << RESTendl;

    // Separation of each observable, independent of the method
    fVariableSeparation.clear();
    for (size_t i = 0; i < fObservables.size(); i++) {
        const auto range = Range(signalValues[i], backgroundValues[i]);
        fVariableSeparation.push_back(Separation(*MakeHistogram("s", "", signalValues[i], range),
                                                 *MakeHistogram("b", "", backgroundValues[i], range)));
    }

    // Training and evaluation, the files of TMVA in memory and in a temporary directory
    const bool debug = fVerboseLevel >= TRestLogManager::REST_Verbose_Level::REST_Debug;
    auto signalTree = MakeTree("Signal", fObservables, signalValues);
    auto backgroundTree = MakeTree("Background", fObservables, backgroundValues);
    WeightsDirectory weightsDirectory;
    TMemFile output(("TMVA_" + fName + ".root").c_str(), "RECREATE");
    // The factory (its methods) destroyed before the loader
    TMVA::DataLoader loader(fName.c_str());
    TMVA::Factory factory(fName.c_str(), &output,
                          std::string(debug ? "!V:!Silent" : "!V:Silent") +
                              ":!Color:!DrawProgressBar:AnalysisType=Classification");
    for (const auto& observable : fObservables) loader.AddVariable(observable.c_str());
    loader.AddSignalTree(signalTree.get(), 1.0);
    loader.AddBackgroundTree(backgroundTree.get(), 1.0);
    loader.PrepareTrainingAndTestTree(TCut(""), TCut(""), fSplitOptions.c_str());
    if (!factory.BookMethod(&loader, fMethod.c_str(), fMethod.c_str(), fOptions.c_str()))
        throw std::runtime_error(context + ": the method " + fMethod + " cannot be booked");
    factory.TrainAllMethods();
    factory.TestAllMethods();
    factory.EvaluateAllMethods();

    auto* method = dynamic_cast<TMVA::MethodBase*>(factory.GetMethod(fName.c_str(), fMethod.c_str()));
    if (!method) throw std::runtime_error(context + ": the method " + fMethod + " was not trained");
    std::ifstream weightFile(method->GetWeightFileName().Data());
    std::stringstream weights;
    weights << weightFile.rdbuf();
    fWeights = weights.str();
    if (fWeights.empty()) {
        throw std::runtime_error(context + ": no weight file " +
                                 std::string(method->GetWeightFileName().Data()));
    }
    auto* bdt = dynamic_cast<TMVA::MethodBDT*>(method);
    fVariableImportance = bdt ? bdt->GetVariableImportance() : std::vector<double>{};

    // ROC curve and score distributions
    fROCIntegral = factory.GetROCIntegral(&loader, fMethod.c_str());
    fROC.reset(factory.GetROCCurve(&loader, fMethod.c_str()));
    if (fROC) fROC->SetName("roc");
    const int signalClass = loader.GetDataSetInfo().GetClassInfo("Signal")->GetNumber();
    const auto test = ReadScores(output.Get<TTree>((fName + "/TestTree").c_str()), fMethod, signalClass);
    const auto train = ReadScores(output.Get<TTree>((fName + "/TrainTree").c_str()), fMethod, signalClass);
    // The same range for the four distributions (as kScoreNames)
    const std::vector<const std::vector<double>*> samples = {&test.first, &test.second, &train.first,
                                                             &train.second};
    std::vector<double> all;
    for (const auto* scores : samples) all.insert(all.end(), scores->begin(), scores->end());
    const auto scoreRange = Range(all, {});
    const std::string title = ";" + fMethod + " score;Entries";
    fScores.clear();
    for (size_t i = 0; i < kScoreNames.size(); i++)
        fScores[kScoreNames[i]] = MakeHistogram(kScoreNames[i], title, *samples[i], scoreRange);
    fSeparation = Separation(*fScores["score_signal"], *fScores["score_background"]);
    fKSSignal = fScores["score_signal"]->KolmogorovTest(fScores["score_signal_train"].get());
    fKSBackground = fScores["score_background"]->KolmogorovTest(fScores["score_background_train"].get());

    RESTInfo << context << ": ROC integral " << fROCIntegral << ", separation " << fSeparation
             << ", Kolmogorov test (training/test) signal " << fKSSignal << " and background "
             << fKSBackground << RESTendl;
    UpdateYAMLFromParams<TRestDataSetTMVA>(fNode);
}

void TRestDataSetTMVA::ReadObjects(TDirectory* directory) {
    fROC.reset();
    fScores.clear();
    if (!directory) return;
    if (auto* roc = directory->Get<TGraph>("roc")) fROC.reset(static_cast<TGraph*>(roc->Clone()));
    for (const auto& name : kScoreNames) {
        if (auto* histogram = directory->Get<TH1D>(name.c_str())) {
            fScores[name].reset(static_cast<TH1D*>(histogram->Clone()));
            fScores[name]->SetDirectory(nullptr);
        }
    }
}

void TRestDataSetTMVA::Apply(TRestDataSet& dataSet) const {
    const std::string context = "TRestDataSetTMVA '" + fName + "'";
    if (fWeights.empty()) throw std::runtime_error(context + ": no weights (not trained)");

    // A reader per slot of the data frame (TMVA::Reader is not thread safe), with the values of its entry
    struct SlotReader {
        std::unique_ptr<TMVA::Reader> reader;
        std::vector<float> values;
        TMVA::MethodBase* method = nullptr;
    };
    const unsigned nSlots = std::max(1u, ROOT::GetThreadPoolSize());
    const bool debug = fVerboseLevel >= TRestLogManager::REST_Verbose_Level::REST_Debug;
    const auto methodType = TMVA::Types::Instance().GetMethodType(fMethod.c_str());
    auto readers = std::make_shared<std::vector<SlotReader>>(nSlots);
    for (auto& slot : *readers) {
        slot.reader = std::make_unique<TMVA::Reader>(debug ? "!Color:!Silent" : "!Color:Silent");
        slot.values.resize(fObservables.size());
        for (size_t i = 0; i < fObservables.size(); i++)
            slot.reader->AddVariable(fObservables[i].c_str(), &slot.values[i]);
        slot.method = dynamic_cast<TMVA::MethodBase*>(slot.reader->BookMVA(methodType, fWeights.c_str()));
        if (!slot.method)
            throw std::runtime_error(context + ": the weights of " + fMethod + " cannot be read");
    }

    // The observables in a vector (hidden column), then the score
    ROOT::RDF::RNode frame = dataSet.GetDataFrame();
    std::string inputs = "ROOT::RVecD{";
    for (size_t i = 0; i < fObservables.size(); i++)
        inputs += (i ? ", " : "") + std::string("static_cast<double>(") + fObservables[i] + ")";
    inputs += "}";
    const std::string inputColumn = fName + "_inputs";
    frame = DefineColumn(frame, inputColumn, inputs);
    auto score = [readers](unsigned slot, const ROOT::RVecD& values) {
        auto& reader = (*readers)[slot];
        for (size_t i = 0; i < reader.values.size(); i++) reader.values[i] = values[i];
        return reader.reader->EvaluateMVA(reader.method);
    };
    const std::string column = GetColumn();
    frame = frame.HasColumn(column) ? frame.RedefineSlot(column, score, {inputColumn})
                                    : frame.DefineSlot(column, score, {inputColumn});
    dataSet.SetDataFrame(frame, {column});
}

void TRestDataSetTMVA::WriteObjects(TDirectory* directory) const {
    if (fROC) directory->WriteObject(fROC.get(), "roc");
    for (const auto& [name, histogram] : fScores) directory->WriteObject(histogram.get(), name.c_str());
}

TCanvas* TRestDataSetTMVA::DrawResults(TCanvas* canvas) const {
    if (!canvas) {
        canvas = new TCanvas((fName + "_results").c_str(), ("TRestDataSetTMVA " + fName).c_str(), 1500, 500);
    } else {
        canvas->Clear();
    }
    canvas->Divide(3, 1);
    const int signalColor = kBlue + 1, backgroundColor = kRed + 1;

    // ROC curve
    canvas->cd(1);
    if (fROC) {
        auto* roc = static_cast<TGraph*>(fROC->Clone());
        roc->SetBit(kCanDelete);
        roc->SetTitle(Form("ROC (test), integral %.4f;Signal efficiency;Background rejection", fROCIntegral));
        roc->SetLineWidth(2);
        roc->Draw("AL");
    } else {
        RESTWarning << "TRestDataSetTMVA '" << fName << "': no ROC curve (trained or read?)" << RESTendl;
    }

    // Score distributions: test filled, training as points, normalized
    canvas->cd(2);
    const auto find = [this](const std::string& name) -> const TH1D* {
        const auto it = fScores.find(name);
        return it == fScores.end() ? nullptr : it->second.get();
    };
    if (find("score_signal") && find("score_background")) {
        auto* legend = new TLegend(0.12, 0.7, 0.6, 0.88);
        legend->SetBit(kCanDelete);
        legend->SetBorderSize(0);
        legend->SetFillStyle(0);
        double maximum = 0;
        std::vector<TH1D*> drawn;
        for (const auto& name : kScoreNames) {
            const TH1D* histogram = find(name);
            if (!histogram) continue;
            auto* copy = static_cast<TH1D*>(histogram->Clone());
            copy->SetDirectory(nullptr);
            copy->SetBit(kCanDelete);
            if (copy->Integral() > 0) copy->Scale(1. / copy->Integral());
            const bool signal = name.find("signal") != std::string::npos;
            const bool train = name.find("train") != std::string::npos;
            const int color = signal ? signalColor : backgroundColor;
            copy->SetLineColor(color);
            copy->SetMarkerColor(color);
            if (train) {
                copy->SetMarkerStyle(20);
                copy->SetMarkerSize(0.6);
            } else {
                copy->SetFillColorAlpha(color, 0.3);
            }
            maximum = std::max(maximum, copy->GetMaximum());
            drawn.push_back(copy);
            const std::string label =
                std::string(signal ? "Signal" : "Background") + (train ? " (training)" : " (test)");
            legend->AddEntry(copy, label.c_str(), train ? "p" : "f");
        }
        for (size_t i = 0; i < drawn.size(); i++) {
            drawn[i]->SetMaximum(1.3 * maximum);
            drawn[i]->SetTitle(Form("KS signal %.3f, background %.3f;%s score;Normalized entries", fKSSignal,
                                    fKSBackground, fMethod.c_str()));
            const bool train = std::string(drawn[i]->GetName()).find("train") != std::string::npos;
            drawn[i]->Draw(i == 0 ? (train ? "E1" : "HIST") : (train ? "E1 SAME" : "HIST SAME"));
        }
        legend->Draw();
    } else {
        RESTWarning << "TRestDataSetTMVA '" << fName << "': no score distributions" << RESTendl;
    }

    // Separation (and importance) of each observable
    canvas->cd(3);
    const int nObservables = static_cast<int>(fObservables.size());
    if (nObservables > 0 && fVariableSeparation.size() == fObservables.size()) {
        const bool importance = fVariableImportance.size() == fObservables.size();
        auto* separation =
            new TH1D((fName + "_variableSeparation").c_str(),
                     importance ? "Observables;;Separation, importance" : "Observables;;Separation",
                     nObservables, 0, nObservables);
        separation->SetDirectory(nullptr);
        separation->SetBit(kCanDelete);
        separation->SetStats(false);
        separation->SetFillColor(kAzure + 1);
        separation->SetBarWidth(importance ? 0.4 : 0.8);
        separation->SetBarOffset(0.1);
        double maximum = 0;
        for (int i = 0; i < nObservables; i++) {
            separation->SetBinContent(i + 1, fVariableSeparation[i]);
            separation->GetXaxis()->SetBinLabel(i + 1, fObservables[i].c_str());
            maximum = std::max(maximum, fVariableSeparation[i]);
            if (importance) maximum = std::max(maximum, fVariableImportance[i]);
        }
        separation->SetMaximum(1.2 * maximum);
        separation->SetMinimum(0);
        gPad->SetBottomMargin(0.3);
        separation->Draw("BAR");
        if (importance) {
            auto* importances =
                static_cast<TH1D*>(separation->Clone((fName + "_variableImportance").c_str()));
            importances->SetDirectory(nullptr);
            importances->SetBit(kCanDelete);
            importances->SetFillColor(kOrange + 1);
            importances->SetBarOffset(0.5);
            for (int i = 0; i < nObservables; i++) importances->SetBinContent(i + 1, fVariableImportance[i]);
            importances->Draw("BAR SAME");
            auto* legend = new TLegend(0.6, 0.78, 0.88, 0.88);
            legend->SetBit(kCanDelete);
            legend->SetBorderSize(0);
            legend->SetFillStyle(0);
            legend->AddEntry(separation, "Separation", "f");
            legend->AddEntry(importances, "Importance (BDT)", "f");
            legend->Draw();
        }
    }
    canvas->cd();
    canvas->Update();
    return canvas;
}
