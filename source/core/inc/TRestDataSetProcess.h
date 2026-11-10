#ifndef TRESTDATASETPROCESS_H
#define TRESTDATASETPROCESS_H

#include <ROOT/RDataFrame.hxx>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TDirectory;
class TFile;
class TRestCut;
class TRestDataSet;
class TRestManager;

/// \class TRestDataSetProcess
/// \brief Process of a TRestDataSetProcessManager: computes its parameters from a reference dataset (e.g.
/// the calibration line from the peaks of a calibration) and adds columns to the dataset processed with them.
///
/// A process works in two steps, called by the manager for each dataset:
/// 1. ObtainParameters: the parameters (results, registered fields) are read from the reference if it was
///    processed by this process, otherwise computed on it (ComputeParameters, implemented by each process:
///    peak fits in TRestDataSetCalibration and TRestDataSetGainMap, PDFs in TRestDataSetLogOdds). The
///    reference is only read, its columns are not changed.
/// 2. Apply: the columns computed with the parameters are added to the dataset processed (e.g. the
///    calibrated energy of a background with the parameters of a calibration).
///
/// Parameters common to all the dataset processes:
/// * referenceFileName: dataset where the parameters are computed, used as it is (the previous processes
///   of the pipeline are not applied to it). Empty: the dataset processed, with the columns of the
///   previous processes. A pattern (* and ?) must match a single dataset, unless referenceSelection is
///   given. `restManager --r <process>=<file>` sets it.
/// * referenceSelection: how a dataset is chosen among those matching referenceFileName: "closest" in time
///   to the dataset processed (the smallest gap between their start and end times, 0 if they overlap;
///   then the closest middle times), e.g. the calibration taken nearest to a background. Empty (default):
///   a single match.
/// * cuts: names of the TRestCut (defined in the manager) applied to the reference.
///
/// If the reference has in its metadata a process with the same name and class (a dataset processed by
/// it), its stored configuration, with its results, is used and nothing is computed.
///
/// Results:
/// * referenceUsed: the dataset used as reference (chosen by referenceSelection).
/// * parametersFileName: the dataset where the parameters were computed. The same as referenceUsed unless
///   they were read from it: then the one where they were originally computed (stored with them), e.g. a
///   background B with a calibration A as reference, A processed with the parameters computed on C:
///   referenceUsed is A and parametersFileName is C.
///
/// The objects of a process (spectra, fits, PDFs) are in the folder RESTDataSetStore/<class>/<name> of the
/// dataset (e.g. RESTDataSetStore/TRestDataSetCalibration/calib), its configuration with the results in
/// RESTMetadataStore as any metadata.
///
/// \code{.yaml}
/// calib:
///   class: TRestDataSetCalibration   # a derived class, with its own keys
///   referenceFileName: "Dataset_Calibration_*.root"
///   referenceSelection: closest
///   cuts: ["energy"]
///   # referenceUsed and parametersFileName are results, written by REST
/// \endcode
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDataSetProcess : public TRestMetadata {
   public:
    /// Folder of the objects of the dataset processes in a dataset file
    inline static const std::string kStoreDirectory = "RESTDataSetStore";

    /// \brief Folder of the objects of the process `name` of class `className` in a file
    /// (RESTDataSetStore/<className>/<name>), nullptr if there is none
    static TDirectory* GetStoreDirectory(TFile* file, const std::string& className, const std::string& name);

   protected:
    /// \brief Computes the parameters (the results, registered fields) of the process on the reference,
    /// called by ObtainParameters when they are not stored in it. The objects written by WriteObjects
    /// (spectra, fits, PDFs) are made here too.
    /// \param reference The reference dataset, e.g. for its metadata (readouts...)
    /// \param frame Its data frame with the cuts applied
    virtual void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) = 0;

    /// \brief Reads the objects (spectra, fits) written by WriteObjects, after the configuration of a
    /// stored process is loaded; nothing by default
    virtual void ReadObjects(TDirectory* directory) { (void)directory; }

    /// \brief `column` as a double column (the C++ functions of the processes take doubles, the
    /// observables can be of other types): the column itself if it is a double, otherwise a cast to
    /// a hidden column (not written)
    std::string AsDouble(ROOT::RDF::RNode& frame, const std::string& column) const;

    /// \brief Defines (or redefines, if it exists: a dataset processed again) a column with a C++ function
    /// of the given columns
    /// \return The new data frame node
    template <class F>
    static ROOT::RDF::RNode DefineColumn(ROOT::RDF::RNode frame, const std::string& name, F function,
                                         const std::vector<std::string>& columns) {
        if (frame.HasColumn(name)) return frame.Redefine(name, function, columns);
        return frame.Define(name, function, columns);
    }
    /// \brief Defines (or redefines) a column from a string expression (JIT compiled by ROOT)
    static ROOT::RDF::RNode DefineColumn(ROOT::RDF::RNode frame, const std::string& name,
                                         const std::string& expression) {
        if (frame.HasColumn(name)) return frame.Redefine(name, expression);
        return frame.Define(name, expression);
    }

    /// \brief Expression (conditions joined by &&) of the TRestCut of the manager given to ObtainParameters,
    /// by name; empty without cuts. Throws if a cut is not in the manager.
    std::string GetCutExpression(const std::vector<std::string>& cutNames) const;

    /// Manager given to ObtainParameters (its TRestCut), nullptr before
    const TRestManager* fManager = nullptr;  //!

    /// \brief Loads a stored configuration of this process (with its results)
    void LoadStoredConfig(const YAML::Node& config);

    /// \brief The reference dataset file: the dataset processed if referenceFileName is empty (empty if it
    /// is not in a file: generated in memory, then computed on it), otherwise the file matching it (chosen by
    /// referenceSelection)
    std::string ResolveReference(const TRestDataSet& dataSet) const;

   public:
    /// Dataset where the parameters are computed; empty: the dataset processed
    std::string fReferenceFileName = "";
    /// How a dataset is chosen among those matching referenceFileName: "closest" (in time), or empty (one)
    std::string fReferenceSelection = "";
    /// Names of the TRestCut of the manager applied to the reference
    std::vector<std::string> fCutNames = {};
    /// Result: the dataset used as reference
    std::string fReferenceUsed = "";
    /// Result: dataset where the parameters were computed (see the class documentation)
    std::string fParametersFileName = "";

    TRestDataSetProcess() = default;  ///< Default constructor.
    /// \brief Constructs from a YAML node; the derived class reads its fields (LoadConfig).
    TRestDataSetProcess(const std::string& instanceName, const YAML::Node& node)
        : TRestMetadata(instanceName, node) {}

    /// \brief Nothing to initialize (see ObtainParameters).
    void Initialize() override {}

    /// \brief Obtains the parameters: those stored in the reference if it was processed by this process,
    /// otherwise computed on the reference with the cuts (ComputeParameters). Called before Apply.
    /// \param dataSet Dataset processed (the reference if referenceFileName is empty)
    /// \param manager Manager with the TRestCut of `cuts`
    void ObtainParameters(const TRestDataSet& dataSet, const TRestManager* manager);

    /// \brief Adds the columns of the process, computed with its parameters (ObtainParameters), to the
    /// dataset
    virtual void Apply(TRestDataSet& dataSet) const = 0;

    /// \brief Writes the objects (spectra, fits, PDFs) in the folder of the process (GetStoreDirectory);
    /// nothing by default
    virtual void WriteObjects(TDirectory* directory) const { (void)directory; }

    /// \brief Loads the process `name` (its class) stored in a file (e.g. a dataset processed by it), with
    /// its objects; the first one of its class if `name` is empty. Throws if it is not found.
    void ReadFromFile(const std::string& fileName, const std::string& name = "");

    /// \brief Writes the configuration with the results (RESTMetadataStore) and the objects (folder
    /// RESTDataSetStore/<class>/<name>, replaced if it exists)
    void Save(TFile* file);

    /// \brief Updates the process stored in a file (a dataset processed by it): Save in the file (e.g.
    /// after a refit of the parameters). The columns of the dataset are not recomputed.
    void UpdateFile(const std::string& fileName);
};

#endif
