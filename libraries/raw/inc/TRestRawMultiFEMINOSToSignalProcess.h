#ifndef TRESTRAWMULTIFEMINOSTOSIGNALPROCESS_H
#define TRESTRAWMULTIFEMINOSTOSIGNALPROCESS_H

#include <cstdio>
#include <optional>
#include <vector>

#include "TRestEventProcess.h"

class TRestRawSignalEvent;

/// \class TRestRawMultiFEMINOSToSignalProcess
/// \brief Decodes the binary files (.aqs) of the Feminos cards, a single card or several through a TCM,
/// into TRestRawSignalEvent.
///
/// The input file is the `inputFileName` of the TRestRun. Its number of events is not known: the file is
/// read until its end (InputFinished), single-thread. The DAQ id of a channel is
/// channel + 72 · (4 · card + chip). The event ID is the event count of the Feminos and its time the start
/// time of the file header plus the 48-bit Feminos timestamp (20 ns ticks). Each signal has 512 time bins:
/// the samples are placed from the time bin given by the data (PFX_TIME_BIN_IX, one per pulse with zero
/// suppression), 0 in the bins not sent; channels without samples are not added.
///
/// No parameters: an event inside a built event (Start/End of Built Event, written by the DAQ also for a
/// single card) ends at its End of Built Event, so with several cards it holds the frames of all of them;
/// an event outside a built event ends at the End of Event of the card.
class TRestRawMultiFEMINOSToSignalProcess : public TRestEventProcess {
   private:
    FILE* fInputBinFile = nullptr;
    long long fFileSize = 0;
    long long fBytesRead = 0;
    bool fFinished = false;

    /// Start time of the run, from the header of the file (s)
    double fStartTime = 0;
    /// Header (Start of Event) of the next event found at the end of a frame of the current one
    std::optional<std::pair<unsigned int, double>> fNextEventHeader;
    bool fFirstEvent = true;

    /// Buffer of the current frame (short words, with its prefix and size)
    std::vector<unsigned short> fFrame;
    /// Inside a built event: the event ends at the End of Built Event, not at the End of Event
    bool fInBuiltEvent = false;

    /// State of the event being decoded
    TRestRawSignalEvent* fEvent = nullptr;
    bool fEventIDSet = false;
    unsigned int fEventCount = 0;

    /// \brief Reads n short words from the file, false at the end of the file.
    bool ReadWords(unsigned short* words, size_t n);
    /// \brief Sets the ID and time of the current event from a Start of Event header.
    void SetEventHeader(unsigned int eventCount, double time);
    /// \brief Decodes the content of a frame (nWords short words after its prefix and size). Returns true
    /// at the End of Event outside a built event.
    bool ReadFrame(const unsigned short* data, size_t nWords);

   public:
    TRestRawMultiFEMINOSToSignalProcess();
    TRestRawMultiFEMINOSToSignalProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawMultiFEMINOSToSignalProcess(const std::string& fileName, const std::string& sectionName);
    ~TRestRawMultiFEMINOSToSignalProcess() override;

    void LoadConfig() override;
    /// \brief Opens the .aqs file and reads its header (start time of the run).
    void InitProcess() override;
    /// \brief Decodes the next event of the file (events without signals are skipped).
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Closes the input file.
    void EndProcess() override;

    bool InputFinished() const override { return fFinished; }
    double GetInputFraction() const override {
        return fFileSize > 0 ? static_cast<double>(fBytesRead) / fFileSize : 0;
    }

    std::string GetInputEvent() const override { return "None"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawMultiFEMINOSToSignalProcess"; }
};

#endif
