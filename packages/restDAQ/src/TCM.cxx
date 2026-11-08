
#include "TCM.h"

#include <filesystem>

#include "FEMINOSPacket.h"

TCM::TCM(RunConfig& rC) : FEMDAQ(rC) {}

TCM::~TCM() {}

void TCM::SendCommand(const char* cmd) {
    auto& TCMProxy = FEMArray.front();

    TCMProxy.Send(cmd);

    uint16_t buf_rcv[8192 / sizeof(uint16_t)];
    // 10 s timeout
    const int length = TCMProxy.Receive(buf_rcv, sizeof(buf_rcv), 10);
    // At least the first word, the error code (buf_rcv[2]) and the reply
    if (length < 6) throw std::runtime_error("TCM reply of " + std::to_string(length) + " bytes");

    WriteLog(TCMProxy, std::string(">> TCM Cmd sent ") + cmd);

    const size_t size = length / sizeof(uint16_t);
    const short errorCode = buf_rcv[2];
    if (errorCode) {
        fprintf(stdout, "--------TCM ERROR---------\n");
        WriteLog(TCMProxy, "--------TCM ERROR---------:");
    }

    FEMINOSPacket::ConfigPacket_Print(&buf_rcv[1], size - 1, stdout);
    std::lock_guard<std::mutex> lock(TCMProxy.logMutex);
    if (TCMProxy.logFile) FEMINOSPacket::ConfigPacket_Print(&buf_rcv[1], size - 1, TCMProxy.logFile);
}
