#ifndef TRESTDETECTORTRANSPORTMANAGER_H
#define TRESTDETECTORTRANSPORTMANAGER_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "TRestDetectorMedium.h"
#include "TRestMetadata.h"

class TFile;

/// \class TRestDetectorTransportTable
/// \brief Charge transport of a medium versus the electric field: the configuration of its tables (the
/// `transport` key of a TRestDetectorTransportManager, computed by restGarfield) and one table (one
/// condition), read from or written to the medium file without Garfield++.
///
/// Configuration: one table per temperature at a single pressure (1 bar by default) over the field grid,
/// added to the medium file (the mediumFile of the TRestDetectorMedium). The transport of a gas depends on
/// E/p, so a table at one pressure is scaled to the others (see below): tables at more pressures are not
/// needed (REST_Garfield_DrawMedium with a pressure compares the scaled tables, to check it). Tables that
/// already exist (same key: pressure, temperature and field range) are skipped unless overwrite is set.
/// temperatures, if empty, are that of the medium; the pressure only applies to gases (other media keep it
/// for the key); nCollisions and maxElectronEnergy are Magboltz parameters, only used for gases.
///
/// usePenning (gases, false by default) applies the Penning transfer to the Townsend coefficient with the
/// transfer probability penningRate, as Garfield++ EnablePenningTransfer: the excitations above the
/// ionisation potential of another component ionise with that probability. It only changes the Townsend
/// coefficient, so drift tables do not need it. Penning tables have their own key (suffix
/// `_Penning<rate>`) and coexist with the tables without Penning.
///
///    transport:
///      pressure: 1 bar
///      temperatures: [293.15]   # K
///      fieldRange: [10 V/cm, 1000 V/cm]
///      fieldNodes: 20
///      logField: true
///      nCollisions: 10
///      maxElectronEnergy: 400 eV
///
/// Medium file, e.g. `ArIso.root`:
/// - `RESTMetadataStore/<medium>`: YAML (TObjString, as the other REST metadata) with the medium (name,
///   state, components) and `tables`, the list of its transport tables: key, pressure, temperature,
///   fieldRange, pressureScaling, penningRate (0 without Penning), carriers and `generator`, the
///   configuration that computed it. Adding a table adds (or replaces) its entry.
/// - `tables/<key>/`: the TGraphs `<medium>_<key>_<carrier>_<quantity>` versus the field in V/cm, with
///   self-contained titles, for each carrier (electron, hole, ion): driftVelocity (mm/us),
///   longitudinalDiffusion and transversalDiffusion (sqrt(cm)), townsend (impact ionisation) and
///   attachment (attachment or trapping) (1/cm).
///
/// The key is the condition and field range, e.g. `1bar_293.15K_E10-1000Vcm` (or
/// `1bar_293.15K_E1000-100000Vcm_Penning0.3`). Gas tables (pressureScaling) are scaled to other pressures
/// as Garfield++ does: the drift velocity depends on E/p, the diffusion coefficients scale as 1/sqrt(p) and
/// the Townsend and attachment coefficients as p. The getters take the field in V/mm and the pressure in
/// bar (REST units); values are interpolated linearly in log(E) (in log of the value for Townsend and
/// attachment) and kept constant out of the range.
class TRestDetectorTransportTable : public TRestMetadata {
   private:
    double Interpolate(const std::vector<double>& values, double field, double pressure,
                       bool logValues) const;

   public:
    /// Directory of the tables in the medium file
    inline static const std::string kTablesDir = "tables";

    // Configuration of the tables (fPressure, below, is the pressure of the tables)
    /// `temperatures` (K) of the tables; empty: that of the medium
    std::vector<double> fTemperatures;
    /// `fieldRange`: range of the electric field (V/mm)
    std::pair<TRestWithUnits, TRestWithUnits> fFieldRange = {1, 100};  // 10-1000 V/cm
    /// `fieldNodes`: number of points of the field grid
    int fFieldNodes = 20;
    /// `logField`: logarithmic (true) or linear field grid
    bool fLogField = true;
    /// `nCollisions`: number of collisions of Magboltz (Garfield++ GenerateGasTable), gases only
    int fCollisions = 10;
    /// `maxElectronEnergy`: Magboltz maximum electron energy, gases only
    TRestWithUnits fMaxElectronEnergy = 0.4;  // 400 eV
    /// Penning transfer in the Townsend coefficient (gases)
    bool fUsePenning = false;
    /// Penning transfer probability, (0, 1]
    double fPenningRate = 0;
    /// `overwrite`: compute again the tables that already exist in the medium file
    bool fOverwrite = false;

    /// Transport parameters of a charge carrier versus fFields
    struct Carrier {
        std::vector<double> fDriftVelocity;          // mm/us
        std::vector<double> fLongitudinalDiffusion;  // sqrt(cm)
        std::vector<double> fTransversalDiffusion;   // sqrt(cm)
        std::vector<double> fTownsend;               // impact ionisation, 1/cm
        std::vector<double> fAttachment;             // attachment or trapping, 1/cm
    };
    /// Carriers a table can have
    inline static const std::set<std::string> kCarriers = {"electron", "hole", "ion"};

    // Table of one condition (filled by restGarfield or Read)
    /// Name of the medium of the table
    std::string fMedium = "";
    /// `pressure` of the tables (configuration) and of one table
    TRestWithUnits fPressure = 1;  // bar
    /// Temperature of one table
    double fTemperature = 293.15;  // K
    /// Gas: the table can be scaled to other pressures
    bool fPressureScaling = true;
    /// Penning transfer probability of the Townsend coefficient, 0 without Penning
    double fPenning = 0;
    /// Electric field at fPressure in V/mm, increasing
    std::vector<double> fFields;
    /// Carriers of the table (electron, hole, ion)
    std::map<std::string, Carrier> fCarriers;

    /// \brief Builds an empty table.
    TRestDetectorTransportTable();
    /// \brief Builds a table configuration from a `transport` node (LoadConfig).
    TRestDetectorTransportTable(const std::string& instanceName, const YAML::Node& node);

    /// \brief Returns the class name, "TRestDetectorTransportTable".
    std::string GetClassName() const override { return "TRestDetectorTransportTable"; }
    /// \brief Reads the configuration; throws if a value is out of range (e.g. fieldRange not 0 < min <
    /// max).
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}

    /// \brief Temperatures (K) of the configuration, those of the medium if empty
    std::vector<double> GetTemperatures(const TRestDetectorMedium& medium) const;
    /// \brief Field grid of the configuration in V/mm
    std::vector<double> GetFieldGrid() const;
    /// \brief Field grid from min to max with nodes points, logarithmic or linear (as
    /// Garfield::Medium::SetFieldGrid)
    static std::vector<double> GetFieldGrid(double minField, double maxField, int nodes, bool logField);

    /// \brief Key of the table, e.g. 1bar_293.15K_E10-1000Vcm, with the suffix _Penning<rate> if penning > 0
    std::string GetKey() const;
    /// \brief Key of a table from its condition: pressure (bar), temperature (K), field range (V/mm) and
    /// Penning rate (0: none).
    static std::string GetKey(double pressure, double temperature, double minField, double maxField,
                              double penning = 0);

    /// \brief Range of electric field (V/mm) of the table at a pressure (bar)
    std::pair<double, double> GetFieldRange(double pressure) const;

    /// \brief Whether the table has the carrier (electron, hole or ion).
    inline bool HasCarrier(const std::string& carrier) const { return fCarriers.count(carrier) > 0; }
    /// \brief Parameters of a carrier. Throws if the table does not have it
    const Carrier& GetCarrier(const std::string& carrier) const;

    /// \brief Drift velocity (mm/us) of a carrier at a field (V/mm) and pressure (bar), interpolated and
    /// scaled as in the class description. Throws if the table does not have the carrier.
    double GetDriftVelocity(double field, double pressure, const std::string& carrier = "electron") const;
    /// \brief Longitudinal diffusion coefficient (sqrt(cm)) at a field (V/mm) and pressure (bar).
    double GetLongitudinalDiffusion(double field, double pressure,
                                    const std::string& carrier = "electron") const;
    /// \brief Transversal diffusion coefficient (sqrt(cm)) at a field (V/mm) and pressure (bar).
    double GetTransversalDiffusion(double field, double pressure,
                                   const std::string& carrier = "electron") const;
    /// \brief Townsend (impact ionisation) coefficient (1/cm) at a field (V/mm) and pressure (bar).
    double GetTownsend(double field, double pressure, const std::string& carrier = "electron") const;
    /// \brief Attachment (or trapping) coefficient (1/cm) at a field (V/mm) and pressure (bar).
    double GetAttachment(double field, double pressure, const std::string& carrier = "electron") const;

    /// \brief Entries of the transport tables of a medium (`tables` of its metadata), empty if none
    static YAML::Node GetTables(TFile* file, const std::string& medium);
    /// \brief Whether a medium file has a table
    static bool HasTable(TFile* file, const std::string& medium, const std::string& key);
    /// \brief Reads a table of a medium file. Throws if it does not exist
    static TRestDetectorTransportTable Read(TFile* file, const std::string& medium, const std::string& key);

    /// \brief Writes the table to a medium file (opened with UPDATE) and adds or replaces its entry in the
    /// metadata of the medium, with this configuration as generator. medium is the definition of the medium
    /// (name, state, components): stored if the file has none, it must agree with the stored one.
    void Write(TFile* file, const YAML::Node& medium, const YAML::Node& generator) const;
};

/// \class TRestDetectorGainTable
/// \brief Avalanche gain of a medium versus the amplification field: the configuration of its tables (the
/// `gain` key of a TRestDetectorTransportManager, computed by restGarfield) and one table (one condition
/// and gap), read from or written to the medium file without Garfield++.
///
/// Configuration: one table per pressure and temperature, for an amplification gap with a uniform field
/// (parallel plates, E = V / gap), over a grid of reduced field E/p (reducedFieldRange, fieldNodes equally
/// spaced): the gain depends on E/p (and p * gap), so the working range in E/p is about the same at every
/// pressure and each pressure p gets the fields E = (E/p) * p. The table is stored versus the field.
/// restGarfield simulates `avalanches` microscopic avalanches per node (Garfield++ AvalancheMicroscopic),
/// each one started by one electron at the mesh side of the gap; the table keeps the mean gain G and the
/// Polya parameter theta from the moments of the avalanche sizes (sigma^2 / G^2 = 1 / (1 + theta)), and the
/// distribution of the sizes per field. pressures and temperatures, if empty, are those of the medium.
/// usePenning and penningRate as in TRestDetectorTransportTable. Tables that already exist (same key) are
/// skipped unless overwrite is set.
///
///    gain:
///      gap: 128 um
///      reducedFieldRange: [25 kV/cm/bar, 35 kV/cm/bar]
///      fieldNodes: 6
///      avalanches: 1000
///      usePenning: true
///      penningRate: 0.3
///
/// Medium file: an entry per table in `amplifications` of the metadata of the medium (key, pressure,
/// temperature, gap, fieldRange, penningRate, avalanches and `generator`) and, in `amplification/<key>/`,
/// the TGraphs `<medium>_<key>_gain` and `<medium>_<key>_theta` versus the field in V/cm and the TH1D
/// `<medium>_<key>_size_<i>` of the avalanche sizes at the field node i. The key is the condition, gap and
/// field range, e.g. `1.4bar_293.15K_gap128um_E25000-35000Vcm_Penning0.3`. The gain does not scale with the
/// pressure (it depends on E/p and p * gap): a table is only used at its pressure and temperature. The
/// getters take the field in V/mm; log G and theta are interpolated linearly in the field, and a field out
/// of the range of the table is an error.
class TRestDetectorGainTable : public TRestMetadata {
   public:
    /// Directory of the tables in the medium file
    inline static const std::string kTablesDir = "amplification";

    // Configuration of the tables
    /// `gap`: amplification gap (mm)
    TRestWithUnits fGap = 0;
    /// `pressures` of the tables (bar); empty: that of the medium
    std::vector<TRestWithUnits> fPressures;
    /// `temperatures` of the tables (K); empty: that of the medium
    std::vector<double> fTemperatures;
    /// Reduced field E/p (V/mm/bar): range and number of equally spaced nodes
    std::pair<TRestWithUnits, TRestWithUnits> fReducedFieldRange = {0, 0};
    /// `fieldNodes`: number of nodes of the reduced field grid
    int fFieldNodes = 5;
    /// Avalanches (one primary electron each) per field node
    int fAvalanches = 1000;
    /// `usePenning`: Penning transfer (gases), as TRestDetectorTransportTable
    bool fUsePenning = false;
    /// `penningRate`: Penning transfer probability, (0, 1]
    double fPenningRate = 0;
    /// `overwrite`: compute again the tables that already exist in the medium file
    bool fOverwrite = false;

    // Table of one condition (filled by restGarfield or Read)
    /// Name of the medium of the table
    std::string fMedium = "";
    double fPressure = 1;          ///< Pressure of one table (bar)
    double fTemperature = 293.15;  ///< Temperature of one table (K)
    double fTableGap = 0;          ///< Gap of one table (mm)
    /// Penning transfer probability, 0 without Penning
    double fPenning = 0;
    /// Amplification field in V/mm, increasing
    std::vector<double> fFields;
    /// Mean gain at each field
    std::vector<double> fGain;
    /// Polya parameter at each field
    std::vector<double> fTheta;

    /// \brief Builds an empty table.
    TRestDetectorGainTable();
    /// \brief Builds a table configuration from a `gain` node (LoadConfig).
    TRestDetectorGainTable(const std::string& instanceName, const YAML::Node& node);

    /// \brief Returns the class name, "TRestDetectorGainTable".
    std::string GetClassName() const override { return "TRestDetectorGainTable"; }
    /// \brief Reads the configuration; throws if a value is out of range (gap <= 0, reducedFieldRange not
    /// 0 < min < max, avalanches < 2...).
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}

    /// \brief Pressures (bar) of the configuration, those of the medium if empty
    std::vector<double> GetPressures(const TRestDetectorMedium& medium) const;
    /// \brief Temperatures (K) of the configuration, those of the medium if empty
    std::vector<double> GetTemperatures(const TRestDetectorMedium& medium) const;
    /// \brief Field grid of the configuration in V/mm at a pressure (bar): the reduced field grid times the
    /// pressure
    std::vector<double> GetFieldGrid(double pressure) const;

    /// \brief Key of the table, e.g. 1.4bar_293.15K_gap128um_E25000-35000Vcm (gap in mm, fields in V/mm),
    /// with the suffix _Penning<rate> if penning > 0
    static std::string GetKey(double pressure, double temperature, double gap, double minField,
                              double maxField, double penning = 0);
    /// \brief Key of this table (its condition, gap, field range and Penning rate).
    std::string GetKey() const;

    /// \brief Range of amplification field (V/mm) of the table
    std::pair<double, double> GetFieldRange() const;
    /// \brief Whether the field (V/mm) is in the range of the table
    bool InRange(double field) const;
    /// \brief Mean gain at a field (V/mm). Throws out of the range of the table
    double GetGain(double field) const;
    /// \brief Polya parameter theta at a field (V/mm). Throws out of the range of the table
    double GetTheta(double field) const;

    /// \brief Entries of the gain tables of a medium (`amplifications` of its metadata), empty if none
    static YAML::Node GetTables(TFile* file, const std::string& medium);
    /// \brief Whether a medium file has a gain table
    static bool HasTable(TFile* file, const std::string& medium, const std::string& key);
    /// \brief Reads a table of a medium file. Throws if it does not exist
    static TRestDetectorGainTable Read(TFile* file, const std::string& medium, const std::string& key);
    /// \brief Writes the table to a medium file (opened with UPDATE) and adds or replaces its entry in the
    /// metadata of the medium, as TRestDetectorTransportTable::Write. sizes are the avalanche sizes at each
    /// field, stored as histograms.
    void Write(TFile* file, const YAML::Node& medium, const YAML::Node& generator,
               const std::vector<std::vector<double>>& sizes) const;
};

/// \class TRestDetectorTransportManager
/// \brief Top section of a restGarfield configuration (as TRestGeant4Manager for restG4): a medium
/// (TRestDetectorMedium, the `medium` key) and the tables to compute for it, added to its mediumFile:
/// `transport` (TRestDetectorTransportTable) and `gain` (TRestDetectorGainTable), keys without class, each
/// one a table configuration or a list of them. restGarfield computes the transport tables first, then the
/// gain tables. The classes do not depend on Garfield++.
///
///    transportManager:
///      class: "TRestDetectorTransportManager"
///      medium:
///        class: "TRestDetectorMedium"
///        name: "ArIso"
///        components: {Ar: 99, iC4H10: 1}
///      transport:
///        fieldRange: [10 V/cm, 1000 V/cm]
///      gain:
///        pressures: [1 bar, 1.4 bar]
///        gap: 128 um
///        reducedFieldRange: [25 kV/cm/bar, 35 kV/cm/bar]
class TRestDetectorTransportManager : public TRestMetadata {
   public:
    /// Configuration of the medium and of the tables (`medium`, `transport` and `gain` keys)
    YAML::Node fMediumNode;
    /// `transport`: a transport table configuration or a list of them
    YAML::Node fTransportNode;
    /// `gain`: a gain table configuration or a list of them
    YAML::Node fGainNode;

    /// Medium created from fMediumNode
    std::unique_ptr<TRestDetectorMedium> fMedium;  //!
    /// Transport table configurations created from fTransportNode
    std::vector<std::unique_ptr<TRestDetectorTransportTable>> fTransport;  //!
    /// Gain table configurations created from fGainNode
    std::vector<std::unique_ptr<TRestDetectorGainTable>> fGain;  //!

    /// \brief Builds the manager from an in-memory YAML section (LoadConfig).
    TRestDetectorTransportManager(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the manager from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorTransportManager(const std::string& fileName, const std::string& sectionName);
    ~TRestDetectorTransportManager() override;

    /// \brief Returns the class name, "TRestDetectorTransportManager".
    std::string GetClassName() const override { return "TRestDetectorTransportManager"; }
    /// \brief Creates the medium and the table configurations. Throws without medium
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Prints the medium and the configuration of each table.
    void PrintMetadata() const override;
};

#endif
