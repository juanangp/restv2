#pragma once

#include "FEMDAQ.h"

/// \file FEMDAQARCFEM.h
/// \brief Data taking of the ARC and FEMINOS electronics.

/// \class FEMDAQARCFEM
/// \brief Data taking of ARC and FEMINOS FEMs (one or several), each with its own UDP socket.
///
/// Threads: a receiver thread per FEM, started by the constructor and running until the destructor
/// (FEMReceiverThread: data frames to FEMProxy::tmpBuffer, other frames are command replies or
/// monitoring); during startDAQ a request thread per active FEM (SendDAQCmdThread, request credits), the
/// event builder (EventBuilder) and the update thread of FEMDAQ. The decoding of the frames is the one of
/// ARCPacket or FEMINOSPacket (packetAPI).
class FEMDAQARCFEM : public FEMDAQ {
   public:
    /// \brief The decoding functions of the electronics (ARCPacket or FEMINOSPacket)
    struct PacketAPI {
        bool (*isMFrame)(uint16_t*);     ///< Whether a frame is a monitoring frame
        bool (*isDataFrame)(uint16_t*);  ///< Whether a frame is a data frame
        /// Extracts the next complete event of the buffer
        bool (*TryExtractNextEvent)(std::deque<uint16_t>&, size_t&, std::vector<uint16_t>&);
        /// Adds the signals of an event to the event, with its timestamp and event counter
        void (*ParseEventFromWords)(std::vector<uint16_t>&, TRestRawSignalEvent&, uint64_t&, uint32_t&);
        void (*DataPacket_Print)(uint16_t*, const uint16_t&, FILE*);    ///< Prints a data/monitoring frame
        void (*ConfigPacket_Print)(uint16_t*, const uint16_t&, FILE*);  ///< Prints a command reply
    };

    PacketAPI packetAPI;  ///< Decoding of the electronics of the configuration

    static std::atomic<bool> stopReceiver;  ///< Ends the receiver threads (destructor)

    /// Frames requested in the first request of a run (credits); limits the burst that the switch and the
    /// socket buffers have to absorb
    std::atomic<uint32_t> kReqFrames{8};
    /// Pending frames needed to send a new request (of this number of frames)
    std::atomic<uint32_t> kFrameThr{2};
    /// Lost frames (FEMINOS sequence numbers) that trigger a resync of the credits
    uint8_t kLostThr = 1;

    /// \brief Default credits: 8 frames requested, a new request every 2 frames
    virtual void CreditsRestore() override { CreditsRestore(8, 2); }

    /// \brief Sets the credits (used by the next startDAQ) and shows them
    virtual void CreditsRestore(uint32_t reqFrames, uint32_t frameThr) override {
        kReqFrames = reqFrames;
        kFrameThr = frameThr;
        CreditsShow();
    }

    /// \brief Writes the credits (stdout and the logs of the active FEMs)
    virtual void CreditsShow() override;

    std::vector<std::thread> receiverThreads;  ///< A receiver thread per FEM

    /// \brief Selects the decoding of the electronics (ARC or FEMINOS, throws for another one) and starts
    /// the receiver threads
    explicit FEMDAQARCFEM(RunConfig& rC);
    /// \brief Stops the run and the receiver threads
    ~FEMDAQARCFEM();

    /// \brief Starts the event builder and the update thread, and a request thread per active FEM
    /// (SendDAQCmdThread) until the end of the run; then stops the threads (stopDAQ)
    virtual void startDAQ(const std::vector<std::string>& flags) override;
    /// \brief Sends the command to all the active FEMs in parallel and waits for their replies; throws if
    /// a FEM fails or does not reply in 10 s
    virtual void SendCommand(const char* cmd) override;

    /// \brief Function of the receiver thread of a FEM: data frames to its tmpBuffer (credits and, for
    /// FEMINOS, lost frames from the sequence numbers); other frames are command replies (cmd_rcv), written
    /// in the log
    void FEMReceiverThread(FEMProxy& FEM);
    /// \brief Function of the event builder thread: builds each event with the event of every active FEM,
    /// checks the sync between the FEMs (timestamps within 1 us, the event ID and time are those of the
    /// majority) and writes the events with signals. When stopEventBuilder is set it writes the data left
    /// (at most ~250 ms) and ends with a sync summary
    void EventBuilder();
    /// \brief Sends a command to a FEM
    /// \param wait wait (10 s) for a reply and log the command; false for the `daq` requests
    void SendCommand(const char* cmd, FEMProxy& FEM, bool wait = true);
    /// \brief Waits (10 s timeout) for a reply of the FEM: cmd_rcv (frames that are not data) above the
    /// value it had before sending the command
    void waitForCmd(FEMProxy& FEM, uint32_t repliesBefore);
    /// \brief Request thread of a FEM during the run: sends `daq` requests (credits) as the frames arrive,
    /// and `daq 0` at the end of the run
    void SendDAQCmdThread(FEMProxy& FEM);

    /// \brief Writes a frame (monitoring, data or command reply) in the log of the FEM, and in stdout
    /// with the info verbose level
    void PrintMonitoring(uint16_t* buff, const uint16_t& size, FEMProxy& FEM);
};
