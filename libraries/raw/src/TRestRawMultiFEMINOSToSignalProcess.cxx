#include "TRestRawMultiFEMINOSToSignalProcess.h"

#include <algorithm>
#include <filesystem>

#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

using namespace std;

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawMultiFEMINOSToSignalProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawMultiFEMINOSToSignalProcess>(instanceName, params);
        });
    return true;
}();

// Prefix codes of the Feminos data format (frame.h of the Feminos mclient)
// 8-bit content
constexpr unsigned short PFX_8_BIT_CONTENT_MASK = 0xFF00;
constexpr unsigned short PFX_ASCII_MSG_LEN = 0x0100;
// 9-bit content
constexpr unsigned short PFX_9_BIT_CONTENT_MASK = 0xFE00;
constexpr unsigned short PFX_START_OF_DFRAME = 0x0800;
constexpr unsigned short PFX_START_OF_MFRAME = 0x0600;
constexpr unsigned short PFX_START_OF_CFRAME = 0x0400;
constexpr unsigned short PFX_TIME_BIN_IX = 0x0E00;
// 0-bit content
constexpr unsigned short PFX_0_BIT_CONTENT_MASK = 0xFFFF;
constexpr unsigned short PFX_END_OF_FRAME = 0x000F;
constexpr unsigned short PFX_START_OF_BUILT_EVENT = 0x0009;
constexpr unsigned short PFX_END_OF_BUILT_EVENT = 0x0008;
constexpr unsigned short PFX_SOBE_SIZE = 0x0006;
// 14-bit content
constexpr unsigned short PFX_14_BIT_CONTENT_MASK = 0xC000;
constexpr unsigned short PFX_CARD_CHIP_CHAN_HIT_IX = 0xC000;
// 12-bit content
constexpr unsigned short PFX_12_BIT_CONTENT_MASK = 0xF000;
constexpr unsigned short PFX_ADC_SAMPLE = 0x3000;
// 4-bit content
constexpr unsigned short PFX_4_BIT_CONTENT_MASK = 0xFFF0;
constexpr unsigned short PFX_START_OF_EVENT = 0x00F0;
constexpr unsigned short PFX_END_OF_EVENT = 0x00E0;

constexpr unsigned short GetAsciiLength(unsigned short w) { return w & 0x00FF; }
constexpr unsigned short GetEventType(unsigned short w) { return w & 0x0007; }
constexpr unsigned short GetEndOfEventSize(unsigned short w) { return w & 0x000F; }
constexpr unsigned short GetCard(unsigned short w) { return (w & 0x3E00) >> 9; }
constexpr unsigned short GetChip(unsigned short w) { return (w & 0x0180) >> 7; }
constexpr unsigned short GetChannel(unsigned short w) { return w & 0x007F; }
constexpr unsigned short GetADCData(unsigned short w) { return w & 0x0FFF; }
constexpr unsigned short GetTimeBin(unsigned short w) { return w & 0x01FF; }

// The size of a frame (bytes) is a short word
constexpr size_t kMaxFrameWords = 0x10000 / 2;
// Channels per chip and chips per card of the DAQ id
constexpr int kChannelsPerChip = 72;
constexpr int kChipsPerCard = 4;
// Time bins of a signal (SCA cells of AGET, 511 for AFTER): the samples are placed at their time bin, 0 in
// the bins not sent (zero suppression), as FEMDAQ
constexpr int kSignalBins = 512;
// Period of the Feminos timestamp (s)
constexpr double kTimestampTick = 2e-8;
}  // namespace

TRestRawMultiFEMINOSToSignalProcess::TRestRawMultiFEMINOSToSignalProcess() : TRestEventProcess() {
    fName = "TRestRawMultiFEMINOSToSignalProcess";
}

TRestRawMultiFEMINOSToSignalProcess::TRestRawMultiFEMINOSToSignalProcess(const std::string& instanceName,
                                                                         const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawMultiFEMINOSToSignalProcess::TRestRawMultiFEMINOSToSignalProcess(const std::string& fileName,
                                                                         const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

TRestRawMultiFEMINOSToSignalProcess::~TRestRawMultiFEMINOSToSignalProcess() {
    if (fInputBinFile) fclose(fInputBinFile);
}

void TRestRawMultiFEMINOSToSignalProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawMultiFEMINOSToSignalProcess::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawMultiFEMINOSToSignalProcess>(fNode);
    UpdateYAMLFromParams<TRestRawMultiFEMINOSToSignalProcess>(fNode);
}

bool TRestRawMultiFEMINOSToSignalProcess::ReadWords(unsigned short* words, size_t n) {
    if (fread(words, sizeof(unsigned short), n, fInputBinFile) != n) return false;
    fBytesRead += n * sizeof(unsigned short);
    return true;
}

void TRestRawMultiFEMINOSToSignalProcess::InitProcess() {
    const std::string inputFileName = fRunInfo ? fRunInfo->GetInputFileName() : "";
    if (inputFileName.empty() || inputFileName == "Null") {
        throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: the TRestRun has no inputFileName");
    }
    if (std::filesystem::path(inputFileName).extension() != ".aqs") {
        throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: the input file is not a .aqs file: " +
                                 inputFileName);
    }

    fInputBinFile = fopen(inputFileName.c_str(), "rb");
    if (!fInputBinFile) {
        throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: cannot open " + inputFileName);
    }
    fFileSize = static_cast<long long>(std::filesystem::file_size(inputFileName));
    fBytesRead = 0;
    fFinished = false;
    fFirstEvent = true;
    fNextEventHeader.reset();
    fInBuiltEvent = false;
    fFrame.assign(kMaxFrameWords, 0);

    // Header: prefix of an ASCII string and the start time of the run (int, s)
    unsigned short prefix = 0;
    if (!ReadWords(&prefix, 1)) {
        throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: cannot read the header of " +
                                 inputFileName);
    }
    if ((prefix & PFX_8_BIT_CONTENT_MASK) != PFX_ASCII_MSG_LEN) {
        throw std::runtime_error(
            "TRestRawMultiFEMINOSToSignalProcess: missing string prefix in the header of " + inputFileName);
    }
    RESTDebug << "Header string length: " << GetAsciiLength(prefix) << RESTendl;

    int startTime = 0;
    if (fread(&startTime, sizeof(int), 1, fInputBinFile) != 1) {
        throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: cannot read the start time of " +
                                 inputFileName);
    }
    fBytesRead += sizeof(int);
    fStartTime = startTime;
    RESTDebug << "Start time of the run: " << startTime << RESTendl;
}

void TRestRawMultiFEMINOSToSignalProcess::SetEventHeader(unsigned int eventCount, double time) {
    fEvent->SetID(static_cast<int>(eventCount));
    fEvent->SetTime(time);
    fEventIDSet = true;
    fEventCount = eventCount;

    // The first event gives the start time of the run, the last one its end time
    if (fFirstEvent) {
        fRunInfo->SetStartTimeStamp(time);
        fFirstEvent = false;
    }
    fRunInfo->SetEndTimeStamp(time);
}

bool TRestRawMultiFEMINOSToSignalProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    auto& signalEvent = dynamic_cast<TRestRawSignalEvent&>(output);
    fEvent = &signalEvent;

    // Events without signals are skipped
    while (true) {
        signalEvent.Initialize();
        fEventIDSet = false;
        if (fNextEventHeader) {
            SetEventHeader(fNextEventHeader->first, fNextEventHeader->second);
            fNextEventHeader.reset();
        }

        bool endOfEvent = false;
        while (!endOfEvent) {
            unsigned short* words = fFrame.data();
            if (!ReadWords(words, 1)) {
                RESTDebug << "End of file reached" << RESTendl;
                fFinished = true;
                return false;
            }

            const unsigned short word = words[0];
            if (word == PFX_START_OF_BUILT_EVENT) {
                RESTDebug << "Start of built event" << RESTendl;
                fInBuiltEvent = true;
            } else if (word == PFX_SOBE_SIZE) {
                // Start of built event with its size (two short words); its frames are read one by one
                if (!ReadWords(words + 1, 2)) {
                    throw std::runtime_error(
                        "TRestRawMultiFEMINOSToSignalProcess: cannot read the size of a built event");
                }
                RESTDebug << "Start of built event of "
                          << ((static_cast<unsigned int>(words[2]) << 16) | words[1]) << " bytes" << RESTendl;
                fInBuiltEvent = true;
            } else if (word == PFX_END_OF_BUILT_EVENT) {
                RESTDebug << "End of built event" << RESTendl;
                fInBuiltEvent = false;
                endOfEvent = true;
            } else if ((word & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_DFRAME ||
                       (word & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_CFRAME ||
                       (word & PFX_9_BIT_CONTENT_MASK) == PFX_START_OF_MFRAME) {
                // Size of the frame in bytes, with its prefix and size
                if (!ReadWords(words + 1, 1)) {
                    throw std::runtime_error(
                        "TRestRawMultiFEMINOSToSignalProcess: cannot read the size of a frame");
                }
                const size_t frameWords = words[1] / 2;
                if (frameWords < 2) {
                    throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: frame of " +
                                             std::to_string(words[1]) + " bytes");
                }
                if (!ReadWords(words + 2, frameWords - 2)) {
                    RESTWarning
                        << "TRestRawMultiFEMINOSToSignalProcess: incomplete frame at the end of the file"
                        << RESTendl;
                    fFinished = true;
                    return false;
                }
                endOfEvent = ReadFrame(words + 2, frameWords - 2);
            } else {
                throw std::runtime_error("TRestRawMultiFEMINOSToSignalProcess: cannot interpret short word " +
                                         std::to_string(word));
            }
        }

        RESTDebug << "Event ID " << signalEvent.GetID() << ", time " << signalEvent.GetTime() << ", "
                  << signalEvent.GetNumberOfSignals() << " signals" << RESTendl;

        if (signalEvent.GetNumberOfSignals() > 0) return true;
        RESTWarning << "TRestRawMultiFEMINOSToSignalProcess: event " << signalEvent.GetID()
                    << " without signals, skipped" << RESTendl;
    }
}

bool TRestRawMultiFEMINOSToSignalProcess::ReadFrame(const unsigned short* data, size_t nWords) {
    bool endOfEvent = false;

    const unsigned short* p = data;
    const unsigned short* end = data + nWords;

    RESTDebug << "Frame of " << nWords << " short words" << RESTendl;

    int signalID = -1;
    std::vector<short> samples(kSignalBins, 0);
    int timeBin = 0;
    bool hasSamples = false;
    // Channels without samples (all removed by the zero suppression) are not added
    auto addSignal = [&]() {
        if (signalID >= 0 && hasSamples) fEvent->AddSignal(signalID, samples);
    };

    while (p < end) {
        const unsigned short word = *p;
        if ((word & PFX_14_BIT_CONTENT_MASK) == PFX_CARD_CHIP_CHAN_HIT_IX) {
            addSignal();
            const int card = GetCard(word);
            const int chip = GetChip(word);
            signalID = GetChannel(word) + kChannelsPerChip * (kChipsPerCard * card + chip);
            std::fill(samples.begin(), samples.end(), 0);
            timeBin = 0;
            hasSamples = false;
            RESTDebug << "Card " << card << " chip " << chip << " DAQ id " << signalID << RESTendl;
            p++;
        } else if ((word & PFX_12_BIT_CONTENT_MASK) == PFX_ADC_SAMPLE) {
            if (signalID >= 0 && timeBin < kSignalBins) {
                samples[timeBin] = static_cast<short>(GetADCData(word));
                hasSamples = true;
            }
            timeBin++;
            p++;
        } else if ((word & PFX_9_BIT_CONTENT_MASK) == PFX_TIME_BIN_IX) {
            // Time bin of the samples that follow (zero suppression: one per pulse)
            timeBin = GetTimeBin(word);
            p++;
        } else if ((word & PFX_4_BIT_CONTENT_MASK) == PFX_START_OF_EVENT) {
            if (end - p < 6) break;
            RESTDebug << "Start of event (type " << GetEventType(word) << ")" << RESTendl;
            // Timestamp (lower, middle, upper 16 bits) and event count (lower, upper 16 bits)
            const unsigned short t0 = p[1], t1 = p[2], t2 = p[3];
            const unsigned int eventCount = (static_cast<unsigned int>(p[5]) << 16) | p[4];
            p += 6;
            // 48-bit timestamp, 16 LSBs first (Feminos manual, data frame); legacy REST used 2^31 and 2^15
            const unsigned long long timestamp = (static_cast<unsigned long long>(t2) << 32) |
                                                 (static_cast<unsigned long long>(t1) << 16) | t0;
            const double time = fStartTime + timestamp * kTimestampTick;

            // The header of the next event can be at the end of a frame of the current one
            if (!fEventIDSet)
                SetEventHeader(eventCount, time);
            else if (eventCount != fEventCount)
                fNextEventHeader = std::make_pair(eventCount, time);
        } else if ((word & PFX_4_BIT_CONTENT_MASK) == PFX_END_OF_EVENT) {
            if (end - p < 2) break;
            const unsigned int size = (static_cast<unsigned int>(GetEndOfEventSize(word)) << 16) + p[1];
            RESTDebug << "End of event (" << size << " bytes)" << RESTendl;
            p += 2;
            if (!fInBuiltEvent) endOfEvent = true;
        } else if ((word & PFX_0_BIT_CONTENT_MASK) == PFX_END_OF_FRAME) {
            RESTDebug << "End of frame" << RESTendl;
            p++;
            break;
        } else {
            // Words not used (hit counts, last cell read, ...)
            p++;
        }
    }
    addSignal();

    return endOfEvent;
}

void TRestRawMultiFEMINOSToSignalProcess::EndProcess() {
    if (fInputBinFile) {
        fclose(fInputBinFile);
        fInputBinFile = nullptr;
    }
}
