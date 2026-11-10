#ifndef TRESTRAWDAQMETADATA_H
#define TRESTRAWDAQMETADATA_H

#include <cstdint>
#include <string>
#include <vector>

#include "TRestMetadata.h"
#include "TRestSystemOfUnits.h"

/// \class TRestRawDAQFEM
/// \brief One FEM of a TRestRawDAQMetadata: its id, IP and, for DCC electronics, its FECs.
///
/// One entry of the `fems` list, YAML keys `id`, `IP` and `FECs`.
class TRestRawDAQFEM : public TRestMetadata {
   public:
    /// FEM id (YAML `id`), stored in the events as TRestRawDAQSignalEvent::GetFEMID
    int fId = 0;
    /// IP address of the FEM (YAML `IP`)
    std::string fIP = "";
    /// FECs read by a DCC (required with electronics DCC)
    std::vector<int> fFECs;

    /// \brief Default constructor (id 0, no IP).
    TRestRawDAQFEM();
    /// \brief Constructor from the YAML node of one `fems` entry.
    TRestRawDAQFEM(const std::string& name, const YAML::Node& node);
    /// \brief Reads `id`, `IP` and `FECs` from the YAML node.
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestRawDAQFEM"; }
};

/// \class TRestRawDAQMetadata
/// \brief Configuration of the data taking of restDAQ (packages/restDAQ), stored in each file it writes.
///
/// The run (numbers, tags, runInfo, output file) is the TRestRun of the same configuration.
/// `updateRate` is raised to 1 s if smaller, and an invalid `maxFileSize` throws in LoadConfig.
///
///     daq:
///       class: "TRestRawDAQMetadata"
///       verbose: "info"           # silent, info or debug (also the `verbose` command of femclient)
///       electronics: "FEMINOS"    # ARC, FEMINOS or DCC
///       fems:
///         - id: 0
///           IP: "192.168.10.2"
///       # DCC: the FECs read by each FEM
///       #   - id: 0
///       #     IP: "192.168.10.13"
///       #     FECs: [0, 1, 2]
///       TCM: "192.168.10.1"       # only for femclient --tcm
///       nEvents: 0                # events of a run, 0: no limit
///       time: 0 s                 # duration of a run, 0: no limit
///       maxFileSize: "1GB"        # a new subrun (file) when reached
///       updateRate: 5 s           # rate printout and Prometheus update
///       prometheusAddress: "0.0.0.0:8080"
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawDAQMetadata : public TRestMetadata {
   public:
    /// Electronics type (YAML `electronics`): ARC, FEMINOS or DCC
    std::string fElectronics = "";
    /// FEMs read (YAML `fems`)
    std::vector<TRestRawDAQFEM> fFEMs;
    /// IP of the TCM, for femclient --tcm
    std::string fTCM = "";
    /// Events of a run, 0: no limit (YAML `nEvents`)
    int fNEvents = 0;
    /// Duration of a run, 0: no limit (YAML `time`)
    TRestWithUnits fTime = 0;  // us
    /// Size of a file that starts a new subrun (YAML `maxFileSize`, see ParseSizeToBytes)
    std::string fMaxFileSize = "1GB";
    /// Period of the rate printout and Prometheus update, at least 1 s (YAML `updateRate`)
    TRestWithUnits fUpdateRate = 5e6;  // us
    /// Address of the Prometheus exporter (YAML `prometheusAddress`)
    std::string fPrometheusAddress = "0.0.0.0:8080";

    /// \brief Default constructor (no configuration).
    TRestRawDAQMetadata();
    /// \brief Constructor from the YAML node of the section.
    TRestRawDAQMetadata(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the section.
    TRestRawDAQMetadata(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered keys, raises updateRate to 1 s and validates maxFileSize (throws).
    void LoadConfig() override;
    void Initialize() override {}
    /// \brief Prints the electronics, the FEMs, the TCM and the run limits.
    void PrintMetadata() const override;
    std::string GetClassName() const override { return "TRestRawDAQMetadata"; }

    /// \brief maxFileSize in bytes (e.g. "100MB", "1 GB", "500kb", a number: bytes)
    uint64_t GetMaxFileSizeBytes() const { return ParseSizeToBytes(fMaxFileSize); }
    /// \brief Duration of a run in seconds (0: no limit)
    double GetMaxTimeSeconds() const { return fTime / 1e6; }
    /// \brief Update rate in seconds
    double GetUpdateRateSeconds() const { return fUpdateRate / 1e6; }

    /// \brief Bytes of a size with units B, kB, MB or GB (case insensitive, powers of 1024)
    ///
    /// The number can have decimals (rounded to bytes, e.g. "1.5GB"); spaces are ignored and k, m, g are
    /// accepted without the B.
    /// \param size Size text, e.g. "500kB" or "1 GB".
    /// \return The size in bytes; throws std::runtime_error without a number or with an unknown unit.
    static uint64_t ParseSizeToBytes(const std::string& size);
};

#endif
