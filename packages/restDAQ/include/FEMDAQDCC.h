#pragma once

#include <chrono>

#include "DCCPacket.h"
#include "FEMDAQ.h"

class FEMDAQDCC : public FEMDAQ {
   public:
    explicit FEMDAQDCC(RunConfig& rC);
    ~FEMDAQDCC();

    virtual void startDAQ(const std::vector<std::string>& flags) override;
    virtual void SendCommand(const char* cmd) override;
    virtual void Pedestals(const std::vector<std::string>& flags) override;

    void EventBuilder();
    /// \brief Trigger and readout loop of startDAQ, until the end of the run
    void AcquisitionLoop(FEMProxy& FEM, const std::vector<int>& fecs, int mode, bool internal,
                         std::chrono::high_resolution_clock::time_point rS);
    bool SendCommand(const char* cmd, FEMProxy& FEM, int pckType = -1);
    void waitForTrigger();
    void PrintMonitoring(DCCPacket::DataPacket* pck);
};
