
#include "FEMDAQDCC.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

FEMDAQDCC::FEMDAQDCC(RunConfig& rC) : FEMDAQ(rC) {}

FEMDAQDCC::~FEMDAQDCC() {
    // The event builder uses the members of this class: stopped before they are destroyed
    stopDAQ();
}

void FEMDAQDCC::Pedestals(const std::vector<std::string>& flags) {
    // Default values
    int nTriggers = 100;
    int mean = 250;
    double stdDev = 5.0;

    //// Flags processing (e.g. "nTriggers=100","mean=250", "stdDev=5.0")
    for (size_t i = 0; i < flags.size(); ++i) {
        const std::string& arg = flags[i];

        if (arg.find("nTriggers=") == 0) {
            nTriggers = std::stoi(arg.substr(10));
        } else if (arg.find("mean=") == 0) {
            mean = std::stoi(arg.substr(5));
        } else if (arg.find("stdDev=") == 0) {
            stdDev = std::stod(arg.substr(7));
        } else {
            std::cout << "Unsupported flag " << arg << " doing nothing!!!" << std::endl;
            std::cout << "Supported flags are: nTriggers=xxx, mean=yyy, stdDev=z.z" << std::endl;
        }
    }

    std::cout << "Starting pedestal run: nTriggers=" << nTriggers << " mean=" << mean << " stdDev=" << stdDev
              << std::endl;

    auto& FEM = FEMArray.front();
    const auto& fecs = runConfig.daq->fFEMs.front().fFECs;
    char cmd[200];

    // Pedestal acquisition
    for (int i = 0; i < nTriggers; i++) {
        SendCommand("isobus 0x6C", FEM);  // SCA start
        SendCommand("isobus 0x1C", FEM);  // SCA stop
        waitForTrigger();
        for (auto fecID : fecs) {
            for (int a = 0; a < 4; a++) {
                sprintf(cmd, "hped acc %d %d %d:%d", fecID, a, 3, 78);
                SendCommand(cmd, FEM);
            }
        }
    }

    SendCommand("fem 0");

    for (auto fecID : fecs) {
        for (int a = 0; a < 4; a++) {
            sprintf(cmd, "hped getsummary %d %d %d:%d", fecID, a, 3, 78);
            SendCommand(cmd, FEM, 1);  // Get summary
            sprintf(cmd, "hped centermean %d %d %d:%d %d", fecID, a, 3, 78, mean);
            SendCommand(cmd, FEM);  // Set mean
            sprintf(cmd, "hped setthr %d %d %d:%d %d %.1f", fecID, a, 3, 78, mean, stdDev);
            SendCommand(cmd, FEM);  // Set thr
        }
    }
}

void FEMDAQDCC::startDAQ(const std::vector<std::string>& flags) {
    int mode = 0;  // Mode
    bool internal = false;

    for (const auto& flag : flags) {
        if (flag == "internal")
            internal = true;  // Internal (random) trigger, otherwise external
                              // (default)
        else if (flag == "ZS")
            mode = 1;  // Zero suppression
        else
            std::cout << "Unknown flag " << flag
                      << " supported flags are internal (trigger) or ZS (Zero suppression)" << std::endl;
    }

    // A previous run not stopped (stopDAQ): stopped now, its threads cannot be replaced while running
    stopDAQ();

    auto& FEM = FEMArray.front();
    FEM.mutex_mem.lock();
    FEM.tmpBuffer.clear();
    FEM.mutex_mem.unlock();

    runStartTime = TRestTools::GetCurrentTime();
    stopRun = false;
    storedEvents = 0;

    stopEventBuilder = false;
    eventBuilderThread = std::thread([this]() { RunThread("Event builder", [this]() { EventBuilder(); }); });
    UpdateRunThread = std::thread([this]() { RunThread("Update thread", [this]() { UpdateThread(); }); });

    const auto& fecs = runConfig.daq->fFEMs.front().fFECs;

    std::cout << TRestTools::GetTimeStampFromUnixTime(runStartTime) << " Starting data taking ";
    if (HasRootFile()) std::cout << fileNameRoot;
    std::cout << std::endl;

    auto rS = std::chrono::high_resolution_clock::now();

    try {
        AcquisitionLoop(FEM, fecs, mode, internal, rS);
    } catch (const std::exception& e) {
        std::cout << "[CRITICAL] DAQ command failed: " << e.what() << std::endl;
        stopDAQ();
        throw;
    }

    // The run ended (nEvents, time or CTRL-C): the event builder writes the data left and stops, so the
    // files can be closed after it
    stopDAQ();
}

void FEMDAQDCC::AcquisitionLoop(FEMProxy& FEM, const std::vector<int>& fecs, int mode, bool internal,
                                std::chrono::high_resolution_clock::time_point rS) {
    char cmd[200];
    while (!stopRun) {
        // SendCommand("fem 0");

        SendCommand("isobus 0x64", FEM);  // SCA start
        if (internal) SendCommand("isobus 0x14", FEM);
        waitForTrigger();
        if (abrt) break;
        // Perform data acquisition phase, compress, accept size
        auto now = std::chrono::high_resolution_clock::now();
        for (auto fecID : fecs) {
            for (int a = 0; a < 4; a++) {
                sprintf(cmd, "areq %d %d %d %d %d", mode, fecID, a, 3, 78);
                SendCommand(cmd, FEM, 0);
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - rS);

        uint64_t ns10 = static_cast<uint64_t>(elapsed.count() / 10) & 0xFFFFFFFFFFFFULL;  // 48 bits mask

        uint16_t t1 = static_cast<uint16_t>((ns10 >> 32) & 0xFFFF);  // Bits 47-32
        uint16_t t2 = static_cast<uint16_t>((ns10 >> 16) & 0xFFFF);  // Bits 31-16
        uint16_t t3 = static_cast<uint16_t>(ns10 & 0xFFFF);          // Bits 15-0

        FEM.mutex_mem.lock();
        FEM.tmpBuffer.emplace_back(0xFFFF);  // Insert word to mark EOE
        FEM.tmpBuffer.emplace_back(t1);      // Insert timeStamp 48 bits
        FEM.tmpBuffer.emplace_back(t2);
        FEM.tmpBuffer.emplace_back(t3);
        FEM.mutex_mem.unlock();
    }
}

void FEMDAQDCC::EventBuilder() {
    auto& FEM = FEMArray.front();

    double lastTimeSaved = runStartTime;
    uint32_t ev_count = 0;
    uint64_t ts = 0x0;
    double eventTime = 0;  // Time of the last event built

    if (HasRootFile()) WriteRunStartTime(runStartTime);

    bool newEvent = false;
    bool emptyBuffer = true;
    int tC = 0;

    std::vector<uint16_t> eventBuffer;
    eventBuffer.reserve(6 * 72 * 4 * 600);

    FEM.bufferIndex = 0;
    FEM.buffer.clear();
    // Signals of an incomplete event of a previous run
    sEvent.Initialize();

    std::cout << "Start of event builder" << std::endl;

    while (!(emptyBuffer && stopEventBuilder)) {
        FEM.mutex_mem.lock();
        if (!FEM.tmpBuffer.empty()) {
            FEM.buffer.insert(FEM.buffer.end(), std::make_move_iterator(FEM.tmpBuffer.begin()),
                              std::make_move_iterator(FEM.tmpBuffer.end()));
            FEM.tmpBuffer.clear();
        }
        FEM.mutex_mem.unlock();

        emptyBuffer = FEM.buffer.empty();

        if (!emptyBuffer) {
            newEvent = DCCPacket::TryExtractNextEvent(FEM.buffer, FEM.bufferIndex, eventBuffer);
        }

        if (newEvent) {
            DCCPacket::ParseEventFromWords(eventBuffer, sEvent, ts, ev_count);
            eventBuffer.clear();
            sEvent.AddFEM(FEM.femID, ev_count, ts, sEvent.GetNumberOfSignals());
            sEvent.SetID(static_cast<int>(ev_count));
            eventTime = (double)ts * 1.E-8 + runStartTime;
            sEvent.SetTime(eventTime);

            // Do not fill empty events
            if (HasRootFile() && sEvent.GetNumberOfSignals() > 0) {
                FillTree(eventTime, lastTimeSaved);

                if (storedEvents % 100 == 0) {
                    CheckFileSize(eventTime);
                }
            }

            newEvent = false;
            eventBuffer.clear();

            if (sEvent.GetNumberOfSignals() > 0) ++storedEvents;
            sEvent.Initialize();
        }

        if (stopEventBuilder) {
            if (tC % 100 == 0) {
                if (runConfig.verboseLevel >= RunConfig::Verbosity::REST_Debug)
                    std::cout << "FEM " << FEM.femID << " Buffer size " << FEM.buffer.size() << std::endl;
            }
            tC++;
            if (tC >= 500) break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << TRestTools::GetTimeStampFromUnixTime(runStartTime)
              << " End of event builder: " << storedEvents << " events stored and " << ev_count
              << " events triggered" << std::endl;

    if (!FEM.buffer.empty()) {
        std::cout << "FEM " << FEM.femID << " Buffer size left " << FEM.buffer.size() << std::endl;
        if (runConfig.verboseLevel >= RunConfig::Verbosity::REST_Debug) {
            std::ostringstream words;
            words << "------DEBUG BUFFER FRAMES LEFT-------";
            char word[8];
            for (const auto& w : FEM.buffer) {
                std::snprintf(word, sizeof(word), "\n%04X", w);
                words << word;
            }
            std::cout << words.str() << std::endl;
            WriteLog(FEM, words.str());
        }
    }

    if (HasRootFile()) {
        // The run ends at the stop of the DAQ or at the last event, if later (as in FEMDAQARCFEM)
        if (eventTime > runEndTime) runEndTime = eventTime;

        WriteRunEndTime(runEndTime);
    }
}

void FEMDAQDCC::SendCommand(const char* cmd) {
    auto& FEM = FEMArray.front();

    if (std::strstr(cmd, "asic") && (std::strstr(cmd, "gain") || std::strstr(cmd, "time"))) {
        uint32_t asicID = 0, gain = 0, time = 0;
        if (std::sscanf(cmd, "asic %u gain 0x%x time 0x%x", &asicID, &gain, &time) == 3) {
            uint8_t regValue = ((time & 0x0F) << 3) | ((gain & 0x03) << 1);
            char finalCmd[64];
            std::snprintf(finalCmd, sizeof(finalCmd), "asic %u write 1 0x%X", asicID, regValue);
            SendCommand(finalCmd, FEM);
        } else {
            std::string error =
                "Invalid cmd sintax: " + std::string(cmd) + " proper sintax is asic 0 gain 0x1 time 0x3";
            throw std::runtime_error(error);
        }
    } else {
        SendCommand(cmd, FEM);
    }
}

bool FEMDAQDCC::SendCommand(const char* cmd, FEMProxy& FEM, int pckType) {
    // pckType = -1 ASCII, >=0 Binary

    FEM.Send(cmd);

    const bool logCmd =
        strncmp(cmd, "wait", 4) != 0 && strncmp(cmd, "areq", 4) != 0 && strncmp(cmd, "isobus", 6) != 0;

    if (logCmd && runConfig.verboseLevel >= RunConfig::Verbosity::REST_Info)
        WriteLog(FEM, ">> FEM " + std::to_string(FEM.femID) + " Cmd sent " + cmd);

    // wait for incoming messages
    bool done = false;
    int length = 0;
    size_t pckCnt = 1;
    // 8192 bytes received; two more words for the '\0' written after a message starting at buf_rcv[1]
    uint16_t buf_rcv[8192 / sizeof(uint16_t) + 2];
    uint8_t* buf_ual;

    while (!done) {
        // 10 s timeout; -1: aborted (CTRL-C)
        length = FEM.Receive(buf_rcv, 8192, 10, &abrt);
        if (length < 0 || abrt) return false;

        // if the first 2 bytes are null, UDP datagram is aligned on next 32-bit
        // boundary, so skip these first two bytes
        int index = 0;
        if ((buf_rcv[0] == 0)) {
            length -= 2;
            index = 1;
        }

        buf_ual = reinterpret_cast<uint8_t*>(&buf_rcv[1]);

        // ERROR ASCII packet
        if (*buf_ual == '-') {
            if (strncmp(cmd, "wait", 4) == 0) return true;
            *(buf_ual + length) = '\0';
            fprintf(stdout, "--------ERROR packet---------: %s\n", buf_ual);
            WriteLog(FEM, "--------ERROR packet---------: " + std::string(reinterpret_cast<char*>(buf_ual)));
            return false;
        }

        if (pckType >= 0) {  // Data packet

            DCCPacket::DataPacket* data_pkt = (DCCPacket::DataPacket*)buf_ual;

            if (GET_TYPE(ntohs(data_pkt->hdr)) == RESP_TYPE_ADC_DATA) {
                // The size of the packet, at most the words received
                const size_t received = (length + 2 * index) / sizeof(uint16_t);
                const size_t pckSize = std::min<size_t>(ntohs(data_pkt->size) / sizeof(uint16_t),
                                                        received > (size_t)index ? received - index : 0);
                FEM.mutex_mem.lock();
                FEM.tmpBuffer.insert(FEM.tmpBuffer.end(), &buf_rcv[index], &buf_rcv[index + pckSize]);
                FEM.mutex_mem.unlock();

                if (runConfig.verboseLevel > RunConfig::Verbosity::REST_Info) PrintMonitoring(data_pkt);
            } else {
                PrintMonitoring(data_pkt);
            }

            // Check End Of Event
            if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV ||
                GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ ||
                (pckType > 0 && pckCnt >= static_cast<size_t>(pckType))) {
                done = true;
            }

        } else {  // ASCII Packet
            if (logCmd) {
                *(buf_ual + length) = '\0';
                if (runConfig.verboseLevel > RunConfig::Verbosity::REST_Info)
                    fprintf(stdout, "dcc().rep(): %s", buf_ual);
                WriteLog(FEM, ">>> dcc().rep(): " + std::string(reinterpret_cast<char*>(buf_ual)));
            }
            done = true;
        }
        pckCnt++;
    }
    return false;
}

void FEMDAQDCC::waitForTrigger() {  // Wait till trigger is acquired
    auto& FEM = FEMArray.front();

    bool retry = false;
    do {
        retry = SendCommand("wait 1000000", FEM);  // Wait for the event to be acquired
    } while (retry && !abrt);  // Infinite loop till aborted or wait succeed
}

void FEMDAQDCC::PrintMonitoring(DCCPacket::DataPacket* pck) {
    char* ptr = nullptr;
    size_t size_mem = 0;

    auto& FEM = FEMArray.front();

    FILE* mem_fp = open_memstream(&ptr, &size_mem);
    if (!mem_fp) return;

    std::string tmstmp = TRestTools::GetTimeStampFromUnixTime(TRestTools::GetCurrentTime());
    fprintf(mem_fp, "--- FEM %d MONI PACKET | %s ---\n", FEM.femID, tmstmp.c_str());

    DataPacket_Print(pck, mem_fp);
    fprintf(mem_fp, "\n");

    fclose(mem_fp);

    if (ptr) {
        {
            std::lock_guard<std::mutex> lock(FEM.logMutex);
            if (FEM.logFile) {
                std::fwrite(ptr, 1, size_mem, FEM.logFile);
                std::fflush(FEM.logFile);
            }
        }

        if (runConfig.verboseLevel >= RunConfig::Verbosity::REST_Info) {
            std::fwrite(ptr, 1, size_mem, stdout);
        }

        free(ptr);
    }
}
