#include "TRestCut.h"

#include <TBranchElement.h>
#include <TLeaf.h>
#include <TTree.h>

#include <cctype>
#include <regex>
#include <stdexcept>

static const bool TRestCut_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestCut>("conditions", &TRestCut::fConditions);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestCut", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestCut>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestCut::TRestCut() : TRestMetadata() { fName = "TRestCut"; }

TRestCut::TRestCut(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestCut::TRestCut(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestCut::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestCut::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestCut>(fNode);
    ReadYAMLVerbose(fNode);
    for (const auto& [name, condition] : fConditions) {
        if (condition.empty()) throw std::runtime_error("TRestCut '" + fName + "': empty condition " + name);
    }
    UpdateYAMLFromParams<TRestCut>(fNode);
}

std::string TRestCut::GetExpression() const {
    std::string expression;
    for (const auto& [name, condition] : fConditions) {
        if (!expression.empty()) expression += " && ";
        expression += "(" + condition + ")";
    }
    return expression;
}

void TRestCut::Compile(TTree* tree) {
    if (!tree) throw std::runtime_error("TRestCut '" + fName + "': no analysis tree");
    if (tree == fTree && fFormula) return;

    fTree = nullptr;
    fLeaves.clear();
    const std::string expression = GetExpression();
    if (expression.empty()) {
        fFormula.reset();
        fTree = tree;
        return;
    }

    // The identifiers that are branches become parameters; the rest (functions, numbers as 1e5) are
    // left to TFormula
    std::map<std::string, size_t> parameters;
    std::string formula;
    const std::regex identifier("[A-Za-z_][A-Za-z0-9_]*");
    size_t last = 0;
    for (auto it = std::sregex_iterator(expression.begin(), expression.end(), identifier);
         it != std::sregex_iterator(); ++it) {
        const size_t position = it->position();
        const std::string name = it->str();
        const bool afterNumber = position > 0 && (std::isdigit(expression[position - 1]) ||
                                                  expression[position - 1] == '.');
        if (afterNumber || !tree->GetBranch(name.c_str())) continue;

        TBranch* branch = tree->GetBranch(name.c_str());
        TLeaf* leaf = tree->GetLeaf(name.c_str());
        if (!leaf || dynamic_cast<TBranchElement*>(branch) || leaf->GetLenStatic() != 1) {
            throw std::runtime_error("TRestCut '" + fName + "': observable " + name +
                                     " is not a scalar (number or boolean)");
        }
        if (parameters.find(name) == parameters.end()) {
            parameters[name] = fLeaves.size();
            fLeaves.push_back(leaf);
        }
        formula += expression.substr(last, position - last) + "[" + std::to_string(parameters[name]) + "]";
        last = position + name.size();
    }
    formula += expression.substr(last);

    fFormula = std::make_unique<TFormula>(("cut_" + fName).c_str(), formula.c_str(), false);
    if (!fFormula->IsValid()) {
        fFormula.reset();
        throw std::runtime_error("TRestCut '" + fName + "': not valid expression (unknown observable?) " +
                                 expression);
    }
    fValues.assign(fLeaves.size(), 0);
    fTree = tree;
}

bool TRestCut::Evaluate() const {
    if (!fTree) throw std::runtime_error("TRestCut '" + fName + "': Compile before Evaluate");
    if (!fFormula) return true;
    // The leaves of the analysis tree point to the variables of the processes (or the input tree buffers)
    for (size_t i = 0; i < fLeaves.size(); i++) fValues[i] = fLeaves[i]->GetValue();
    fFormula->SetParameters(fValues.data());
    return fFormula->Eval(0) != 0;
}
