#pragma once

#include "FEMDAQ.h"

/// \class TCM
/// \brief Commands to the TCM (trigger and clock module), femclient --tcm. No data taking.
class TCM : public FEMDAQ {
   public:
    /// \param rC configuration with isTCM (the TCM IP of the TRestRawDAQMetadata)
    explicit TCM(RunConfig& rC);
    ~TCM();

    /// \brief Not implemented for the TCM (writes a message)
    virtual void startDAQ(const std::vector<std::string>& flags) override {
        std::cout << "Not implemented for TCM" << std::endl;
    }
    /// \brief Sends the command to the TCM and writes its reply (stdout and log); throws on a timeout
    /// (10 s) or a short reply
    virtual void SendCommand(const char* cmd) override;
};
