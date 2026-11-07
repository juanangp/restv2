#ifndef TRESTRAWDAQMETADATA_H
#define TRESTRAWDAQMETADATA_H

#include <cstdint>
#include <string>
#include <vector>

#include "TRestMetadata.h"
#include "TRestSystemOfUnits.h"

/// \brief One FEM of a TRestRawDAQMetadata: its id, IP and, for DCC electronics, its FECs.
class TRestRawDAQFEM : public TRestMetadata {
   public:
    int fId = 0;
    std::string fIP = "";
    /// FECs read by a DCC (required with electronics DCC)
    std::vector<int> fFECs;

    TRestRawDAQFEM();
    TRestRawDAQFEM(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestRawDAQFEM"; }
};

/// \class TRestRawDAQMetadata
/// \brief Configuration of the data taking of restDAQ (packages/restDAQ), stored in each file it writes.
///
/// The run (numbers, tags, runInfo, output file) is the TRestRun of the same configuration.
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
class TRestRawDAQMetadata : public TRestMetadata {
   public:
    std::string fElectronics = "";
    std::vector<TRestRawDAQFEM> fFEMs;
    /// IP of the TCM, for femclient --tcm
    std::string fTCM = "";
    int fNEvents = 0;
    TRestWithUnits fTime = 0;  // us
    std::string fMaxFileSize = "1GB";
    TRestWithUnits fUpdateRate = 5e6;  // us
    std::string fPrometheusAddress = "0.0.0.0:8080";

    TRestRawDAQMetadata();
    TRestRawDAQMetadata(const std::string& instanceName, const YAML::Node& node);
    TRestRawDAQMetadata(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void Initialize() override {}
    void PrintMetadata() const override;
    std::string GetClassName() const override { return "TRestRawDAQMetadata"; }

    /// \brief maxFileSize in bytes (e.g. "100MB", "1 GB", "500kb", a number: bytes)
    uint64_t GetMaxFileSizeBytes() const { return ParseSizeToBytes(fMaxFileSize); }
    double GetMaxTimeSeconds() const { return fTime / 1e6; }
    double GetUpdateRateSeconds() const { return fUpdateRate / 1e6; }

    /// \brief Bytes of a size with units B, kB, MB or GB (case insensitive, powers of 1024)
    static uint64_t ParseSizeToBytes(const std::string& size);
};

#endif
