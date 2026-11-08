#pragma once

#include <chrono>

#include "DCCPacket.h"
#include "FEMDAQ.h"

/// \file FEMDAQDCC.h
/// \brief Data taking of the DCC electronics.

/// \class FEMDAQDCC
/// \brief Data taking of a DCC (the first FEM of the configuration, with its FECs).
///
/// The acquisition is driven by the commands of startDAQ (no receiver thread): for each event it starts
/// the SCA, waits for the trigger and requests the data of each ASIC of the FECs (`areq`); the data
/// packets go to FEMProxy::tmpBuffer followed by an end-of-event mark (0xFFFF) and a 48-bit timestamp
/// (10 ns ticks since the start of the run, taken by the computer), which the event builder decodes.
class FEMDAQDCC : public FEMDAQ {
   public:
    explicit FEMDAQDCC(RunConfig& rC);
    /// \brief Stops the run (stopDAQ)
    ~FEMDAQDCC();

    /// \brief Acquisition until the end of the run
    /// \param flags `internal` (internal trigger, otherwise external) and `ZS` (zero suppression)
    virtual void startDAQ(const std::vector<std::string>& flags) override;
    /// \brief Sends a command to the DCC and waits for its reply. `asic <id> gain 0x<g> time 0x<t>` is
    /// translated into the write of the ASIC register; throws on a wrong syntax or a timeout
    virtual void SendCommand(const char* cmd) override;
    /// \brief Pedestal run: `nTriggers` (100) triggers accumulating the pedestal histograms, then centers
    /// the pedestals at `mean` (250) and sets the thresholds at `stdDev` (5.0) standard deviations
    /// \param flags `nTriggers=N`, `mean=M`, `stdDev=S`
    virtual void Pedestals(const std::vector<std::string>& flags) override;

    /// \brief Function of the event builder thread: extracts the events of the buffer (up to the
    /// end-of-event mark), decodes them and writes the events with signals
    void EventBuilder();
    /// \brief Trigger and readout loop of startDAQ, until the end of the run
    void AcquisitionLoop(FEMProxy& FEM, const std::vector<int>& fecs, int mode, bool internal,
                         std::chrono::high_resolution_clock::time_point rS);
    /// \brief Sends a command to the DCC and reads its reply (10 s timeout)
    /// \param pckType -1 for an ASCII reply; >= 0 for binary data packets, read until the end of event or
    /// request (or pckType packets if > 0), the ADC data go to the buffer of the event builder
    /// \return true only if the command is `wait` and the DCC replied an error (no trigger yet, retry);
    /// false otherwise, also on abort or an error reply
    bool SendCommand(const char* cmd, FEMProxy& FEM, int pckType = -1);
    /// \brief Sends `wait` until the trigger is acquired (or abort)
    void waitForTrigger();
    /// \brief Writes a data packet in the log of the FEM, and in stdout with the info verbose level
    void PrintMonitoring(DCCPacket::DataPacket* pck);
};
