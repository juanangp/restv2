#ifndef TRESTDETECTORTRANSPORT_H
#define TRESTDETECTORTRANSPORT_H

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "TRestMetadata.h"

class TFile;
class TRestDetectorMedium;

/// \class TRestDetectorTransport
/// \brief Charge transport of a medium versus the electric field: the configuration of its tables
/// (section of a TRestDetectorTransportManager, computed by restGarfield) and one table (one condition),
/// read from or written to the medium file without Garfield++.
///
/// Configuration: one table per pressure and temperature over the field grid, added to the medium file
/// `<outputPath>/<medium name>.root`. Tables that already exist (same key: pressure, temperature and field
/// range) are skipped unless overwrite is set. pressures only apply to gases (other media use the first
/// one for the key); nCollisions and maxElectronEnergy are Magboltz parameters, only used for gases.
///
/// usePenning (gases, false by default) applies the Penning transfer to the Townsend coefficient with the
/// transfer probability penningRate, as Garfield++ EnablePenningTransfer: the excitations above the
/// ionisation potential of another component ionise with that probability. It only changes the Townsend
/// coefficient, so avalanche (gain) tables need it but drift tables do not. Penning tables have their own key
/// (suffix `_Penning<rate>`) and coexist with the tables without Penning.
///
///    gasTable:
///      class: "TRestDetectorTransport"
///      pressures: [1 bar, 1.4 bar]
///      temperatures: [293.15]   # K
///      fieldRange: [10 V/cm, 1000 V/cm]
///      fieldNodes: 20
///      logField: true
///      nCollisions: 10
///      maxElectronEnergy: 400 eV
///      usePenning: false          # true for avalanche tables, with penningRate (e.g. 0.3)
///      outputPath: "."            # directory of the medium file, the current one by default
///
/// Medium file, e.g. `ArIso.root`:
/// - `RESTMetadataStore/<medium>`: YAML (TObjString, as the other REST metadata) with the medium (name,
///   state, components) and `tables`, the list of its tables: key, pressure, temperature, fieldRange,
///   pressureScaling, penningRate (0 without Penning), carriers and `generator`, the configuration that
///   computed it. Adding a table adds (or replaces) its entry.
/// - `tables/<key>/`: the TGraphs `<medium>_<key>_<carrier>_<quantity>` versus the field in V/cm, with
///   self-contained titles, for each carrier (electron, hole, ion): driftVelocity (mm/us),
///   longitudinalDiffusion and transversalDiffusion (sqrt(cm)), townsend (impact ionisation) and
///   attachment (attachment or trapping) (1/cm); and `garfieldTable` (TObjString), the Garfield++ table
///   used by restGarfield. Tables without `carriers` (first format) are read as electron tables.
///
/// The key is the condition and field range, e.g. `1bar_293.15K_E10-1000Vcm` (or
/// `1bar_293.15K_E1000-100000Vcm_Penning0.3`). Gas tables
/// (pressureScaling) are scaled to other pressures as Garfield++ does: the drift velocity depends on E/p,
/// the diffusion coefficients scale as 1/sqrt(p) and the Townsend and attachment coefficients as p. The
/// getters take the field in V/mm and the pressure in bar (REST units); values are interpolated linearly
/// in log(E) (in log of the value for Townsend and attachment) and kept constant out of the range.
class TRestDetectorTransport : public TRestMetadata {
   private:
    double Interpolate(const std::vector<double>& values, double field, double pressure,
                       bool logValues) const;

   public:
    inline static const std::string kTablesDir = "tables";

    // Configuration of the tables
    std::vector<TRestWithUnits> fPressures = {1};
    std::vector<double> fTemperatures = {293.15};
    std::pair<TRestWithUnits, TRestWithUnits> fFieldRange = {1, 100};  // 10-1000 V/cm
    int fFieldNodes = 20;
    bool fLogField = true;
    int fCollisions = 10;
    TRestWithUnits fMaxElectronEnergy = 0.4;  // 400 eV
    /// Penning transfer in the Townsend coefficient (gases), for avalanche tables
    bool fUsePenning = false;
    /// Penning transfer probability, (0, 1]
    double fPenningRate = 0;
    /// Directory of the medium file. A leading ~ is $HOME
    std::string fOutputPath = ".";
    bool fOverwrite = false;

    /// Transport parameters of a charge carrier versus fFields
    struct Carrier {
        std::vector<double> fDriftVelocity;          // mm/us
        std::vector<double> fLongitudinalDiffusion;  // sqrt(cm)
        std::vector<double> fTransversalDiffusion;   // sqrt(cm)
        std::vector<double> fTownsend;               // impact ionisation, 1/cm
        std::vector<double> fAttachment;             // attachment or trapping, 1/cm
    };
    inline static const std::set<std::string> kCarriers = {"electron", "hole", "ion"};

    // Table of one condition (filled by restGarfield or Read)
    std::string fMedium = "";
    double fPressure = 1;          // bar
    double fTemperature = 293.15;  // K
    /// Gas: the table can be scaled to other pressures
    bool fPressureScaling = true;
    /// Penning transfer probability of the Townsend coefficient, 0 without Penning
    double fPenning = 0;
    /// Electric field at fPressure in V/mm, increasing
    std::vector<double> fFields;
    /// Carriers of the table (electron, hole, ion)
    std::map<std::string, Carrier> fCarriers;

    TRestDetectorTransport();
    TRestDetectorTransport(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorTransport(const std::string& fileName, const std::string& sectionName);

    std::string GetClassName() const override { return "TRestDetectorTransport"; }
    void LoadConfig() override;
    void Initialize() override {}

    /// \brief Path of the medium file (its directory is created if needed)
    std::filesystem::path GetMediumFile(const TRestDetectorMedium& medium) const;
    /// \brief Field grid of the configuration in V/mm
    std::vector<double> GetFieldGrid() const;

    /// \brief Key of the table, e.g. 1bar_293.15K_E10-1000Vcm, with the suffix _Penning<rate> if penning > 0
    std::string GetKey() const;
    static std::string GetKey(double pressure, double temperature, double minField, double maxField,
                              double penning = 0);

    /// \brief Range of electric field (V/mm) of the table at a pressure (bar)
    std::pair<double, double> GetFieldRange(double pressure) const;

    inline bool HasCarrier(const std::string& carrier) const { return fCarriers.count(carrier) > 0; }
    /// \brief Parameters of a carrier. Throws if the table does not have it
    const Carrier& GetCarrier(const std::string& carrier) const;

    double GetDriftVelocity(double field, double pressure, const std::string& carrier = "electron") const;
    double GetLongitudinalDiffusion(double field, double pressure,
                                    const std::string& carrier = "electron") const;
    double GetTransversalDiffusion(double field, double pressure,
                                   const std::string& carrier = "electron") const;
    double GetTownsend(double field, double pressure, const std::string& carrier = "electron") const;
    double GetAttachment(double field, double pressure, const std::string& carrier = "electron") const;

    /// \brief Entries of the tables of a medium (`tables` of its metadata), empty if there are none
    static YAML::Node GetTables(TFile* file, const std::string& medium);
    /// \brief Whether a medium file has a table
    static bool HasTable(TFile* file, const std::string& medium, const std::string& key);
    /// \brief Reads a table of a medium file. Throws if it does not exist
    static TRestDetectorTransport Read(TFile* file, const std::string& medium, const std::string& key);

    /// \brief Writes the table to a medium file (opened with UPDATE) and adds or replaces its entry in the
    /// metadata of the medium, with this configuration as generator. medium is the YAML of the medium
    /// (name, type, components): it is stored with the first table and must agree with the stored one.
    /// garfieldTable, if not empty, is the Garfield++ table.
    void Write(TFile* file, const YAML::Node& medium, const YAML::Node& generator,
               const std::string& garfieldTable = "") const;
};

#endif
