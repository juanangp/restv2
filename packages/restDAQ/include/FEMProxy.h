#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "FEMSocket.h"

/// \file FEMProxy.h
/// \brief State of a FEM in the data taking.

/// \class FEMProxy
/// \brief A FEM (or the TCM) in the data taking: its socket plus the state shared by the threads (command
/// replies, request credits, received data, event being built, log file).
///
/// The receiver thread appends the data frames to tmpBuffer (under mutex_mem); the event builder moves them
/// to buffer and extracts the events from it.
class FEMProxy : public FEMSocket {
   public:
    FEMProxy() { tmpBuffer.reserve(500 * 72 * 4 * 520); }
    ~FEMProxy() = default;

    /// Neither copied nor moved: kept in a std::deque (FEMDAQ::FEMArray), the threads hold references
    FEMProxy(const FEMProxy&) = delete;
    FEMProxy& operator=(const FEMProxy&) = delete;

    bool pendingEvent = true;  ///< The event builder has not yet extracted the current event of this FEM
    bool active = true;        ///< Receives the commands and takes part in the run (`fem` command)

    std::atomic<uint32_t> cmd_sent{0};    ///< Commands sent that wait for a reply
    std::atomic<uint32_t> cmd_rcv{0};     ///< Replies received (frames that are not data)
    std::atomic<uint32_t> daq_credit{0};  ///< Data frames received not yet requested again (ARC/FEMINOS)
    /// Wakes up the DAQ command thread when frames are received
    std::condition_variable credit_cv;
    std::mutex credit_m;                  ///< Mutex of credit_cv
    std::atomic<uint64_t> framesLost{0};  ///< Data frames lost in the run (FEMINOS sequence numbers)
    int femID = 0;                        ///< ID of the FEM (-1 for the TCM)
    size_t bufferIndex = 0;  ///< Position in buffer up to which the current event has been searched

    /// Event counter of an event without Start-Of-Event header
    static constexpr uint32_t kNoEvCount = 0xFFFFFFFF;
    /// Event counter and timestamp (ticks of 10 ns) of the last event read
    uint32_t evCount = kNoEvCount;
    uint64_t evTs = 0;  ///< Timestamp (ticks of 10 ns) of the last event read

    std::vector<uint16_t> tmpBuffer;  ///< Words received, not yet taken by the event builder
    std::deque<uint16_t> buffer;      ///< Words taken by the event builder, not yet in an event

    FILE* logFile = nullptr;
    /// Guards logFile: the receiver thread writes in it while the commands open and close it
    std::mutex logMutex;

    std::mutex mutex_mem;  ///< Guards tmpBuffer
};
