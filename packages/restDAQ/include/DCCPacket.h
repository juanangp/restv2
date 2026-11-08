#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include <cstdio>
#include <deque>
#include <vector>

#include "TRestRawSignalEvent.h"

/// \file DCCPacket.h
/// \brief Decoding of the packets of the DCC electronics (AFTER/AGET FECs read by a DCC).

/// \name DCC packet format
/// Macros to decode (GET_*) and build (PUT_*) the fields of the headers of the packets (response type,
/// DCC header, read-back arguments, event count, sample flags) and sizes.
///@{
#define MAX_ETH_PACKET_DATA_SIZE 4096

// Response Packet Header Decode Macros
#define GET_REQ_INDEX(word) ((word) & 0x000F)
#define GET_WR_ACK(word) ((word) & 0x0030)
#define GET_RD_ACK(word) ((word) & 0x0040)
#define GET_TR_OK(word) ((word) & 0x0080)
#define GET_TYPE(word) ((word) & 0xF000)
#define PUT_TYPE(word, ty) (((word) & 0x0FFF) | ((ty) & 0xF000))

// Response Packet Types from FEM (see t2k_pkg.vhd for coherence)
#define RESP_TYPE_ADC_DATA 0x0000
#define RESP_TYPE_SLOW_CTRL 0x1000
// These types are not among those returned by the FEM but by the DCC
#define RESP_TYPE_HISTOGRAM 0x2000
#define RESP_TYPE_HISTOSTAT 0x3000
#define RESP_TYPE_HISTOSUMMARY 0x4000
#define GET_RESP_TYPE(word) (((word) & 0xF000) >> 12)
#define GET_RESP_INDEX(word) ((word) & 0x000F)

// Macros to interpret DataPacket header
#define GET_FEC_ERROR(word) (((word) & 0x03F0) >> 4)
#define GET_LOS_FLAG(word) (((word) & 0x0400) >> 10)
#define GET_SYNCH_FAIL(word) (((word) & 0x0800) >> 11)

// Macros to interpret DataPacket read back arguments
#define GET_RB_MODE(word) (((word) & 0x4000) >> 14)
#define GET_RB_COMPRESS(word) (((word) & 0x2000) >> 13)
#define GET_RB_ARG2(word) (((word) & 0x1E00) >> 9)
#define GET_RB_ARG1(word) (((word) & 0x01FF))

// Macros to interpret DataPacket event type / count
#define GET_EVENT_TYPE(word) (((word) & 0xC000) >> 14)
#define GET_EVENT_COUNT(word) (((word) & 0x3FFF))

// Macros to interpret DataPacket Samples
#define CELL_INDEX_FLAG 0x1000
#define GET_CELL_INDEX(word) (((word) & 0x0FFF))
#define ARGUMENT_FLAG 0x2000
#define GET_ARGUMENTS(word) (((word) & 0xDFFF))
#define SAMPLE_COUNT_FLAG 0x4000
#define GET_SAMPLE_COUNT(word) (((word) & 0x0FFF))

// Macros to manipulate DataPacket DCC header
#define FRAME_TYPE_FEM_DATA 0x0000
#define FRAME_TYPE_DCC_DATA 0x0001
#define FRAME_FLAG_EORQ 0x0004
#define FRAME_FLAG_EOEV 0x0008
#define GET_FEM_INDEX(word) (((word) & 0x000F) >> 0)
#define GET_DCC_INDEX(word) (((word) & 0x03F0) >> 4)
#define GET_FRAME_TY_V2(word) (((word) & 0x3C00) >> 10)
#define PUT_FEM_INDEX(word, ix) (((word) & 0xFFF0) | (((ix) & 0x000F) << 0))
#define PUT_DCC_INDEX(word, ix) (((word) & 0xFC0F) | (((ix) & 0x003F) << 4))
#define PUT_FRAME_TY_V2(word, ty) (((word) & 0x03FF) | FRAME_HDR_V2 | (((ty) & 0x000F) << 10))

// We now support the old and new version of the data packet structure
#define FRAME_HDR_V2_FIELD 0xC000
#define FRAME_HDR_V2 0x4000
#define IS_DATA_PACKET_V2(word) (((word) & FRAME_HDR_V2_FIELD) == FRAME_HDR_V2)

// Miscellanea
#define MAX_NB_OF_FEM_PER_DCC 6
#define MAX_EVENT_SIZE 6 * 4 * 79 * 2 * (512 + 32) * 12
#define BHISTO_SIZE 256
#define PHISTO_SIZE 512
#define ASIC_MAX_CHAN_INDEX 78

///@}

/// \brief Packets of the DCC and their decoding. All the fields are in network byte order (ntohs)
namespace DCCPacket {

// New structure of DataPacket. Aliased to DataPacketV2
/// \brief Data packet of a FEM: ADC samples of a channel, or pedestal histograms (by the type of hdr)
typedef struct {
    unsigned short size;    // size in bytes including the size of the size field itself
    unsigned short dcchdr;  // specific header added by the DCC
    unsigned short hdr;     // specific header put by the FEM
    unsigned short args;    // read-back arguments echoed by FEM
    unsigned short ts_h;    // local FEM timestamp MSB
    unsigned short ts_l;    // local FEM timestamp LSB
    unsigned short ecnt;    // local FEM event type/count
    unsigned short scnt;    // sample count in first pulse over thr or total sample
                            // count in packet
    unsigned short samp[MAX_ETH_PACKET_DATA_SIZE / (sizeof(unsigned short))];  // data storage area
} DataPacket;

// End of Event Packet
/// \brief End of event packet of the DCC: bytes received and sent per FEM
typedef struct {
    unsigned short size;    // size in bytes including the size of the size field itself
    unsigned short dcchdr;  // specific header added by the DCC
    unsigned short hdr;     // specific header similar to what is put by the FEM
    unsigned short pad;     // padding for 32-bit alignement
    unsigned int byte_rcv[MAX_NB_OF_FEM_PER_DCC];  // data received per FEM
    unsigned int byte_snd[MAX_NB_OF_FEM_PER_DCC];  // data sent per FEM
    unsigned int tot_byte_rcv;
    unsigned int tot_byte_snd;
} EndOfEventPacket;

/// \brief Histogram packet of the DCC
typedef struct {
    unsigned short size;               // size in bytes including the size of the size field itself
    unsigned short dcchdr;             // specific header added by the DCC
    unsigned short hdr;                // placeholder for header normally put by FEM
    unsigned short min_bin;            // range lower bound
    unsigned short max_bin;            // range higher bound
    unsigned short bin_wid;            // width of one bin in number of units
    unsigned short bin_cnt;            // number of bins
    unsigned short min_val;            // lowest bin with at least one entry
    unsigned short max_val;            // highest bin with at least one entry
    unsigned short mean;               // mean
    unsigned short stddev;             // standard deviation
    unsigned short pad;                // padding for 32-bit alignment
    unsigned int entries;              // number of entries
    unsigned short samp[BHISTO_SIZE];  // data storage area
} HistogramPacket;

/// \brief Pedestal mean and standard deviation of a channel
typedef struct {
    unsigned short mean;   // mean of channel pedestal
    unsigned short stdev;  // standard deviation of channel pedestal
} channel_stat;

// Histogram data packet for pedestals
/// \brief Summary of the pedestals of the channels of an ASIC
typedef struct {
    unsigned short size;                         // size in bytes including the size of the size field itself
    unsigned short dcchdr;                       // specific header added by the DCC
    unsigned short hdr;                          // specific header inherited from FEM
    unsigned short args;                         // read-back arguments as encoded by FEM
    unsigned short scnt;                         // number of unsigned short filled in following array
    channel_stat stat[ASIC_MAX_CHAN_INDEX + 1];  // summary of statistics for current ASIC
} PedestalHistoSummaryPacket;

// Histogram data packet for pedestals
/// \brief Statistics of the pedestal histogram of a channel
typedef struct {
    unsigned short size;     // size in bytes including the size of the size field itself
    unsigned short dcchdr;   // specific header added by the DCC
    unsigned short hdr;      // specific header put by the FEM
    unsigned short args;     // read-back arguments echoed by FEM
    unsigned short min_bin;  // range lower bound
    unsigned short max_bin;  // range higher bound
    unsigned short bin_wid;  // width of one bin in number of units
    unsigned short bin_cnt;  // number of bins
    unsigned short min_val;  // lowest bin with at least one entry
    unsigned short max_val;  // highest bin with at least one entry
    unsigned short mean;     // mean
    unsigned short stddev;   // standard deviation
    unsigned int entries;    // number of entries
    unsigned short bin_sat;  // number of bins saturated
    unsigned short align;    // alignment
} PedestalHistoMathPacket;

// New structure of DataPacket. Aliased to DataPacketV2
/// \brief Bins of the pedestal histogram of a channel
typedef struct {
    unsigned short size;               // size in bytes including the size of the size field itself
    unsigned short dcchdr;             // specific header added by the DCC
    unsigned short hdr;                // specific header put by the FEM
    unsigned short args;               // read-back arguments echoed by FEM
    unsigned short ts_h;               // local FEM timestamp MSB
    unsigned short ts_l;               // local FEM timestamp LSB
    unsigned short ecnt;               // local FEM event type/count
    unsigned short scnt;               // sample count in first pulse over thr or total sample
                                       // count in packet
    unsigned short samp[PHISTO_SIZE];  // data storage area
} PedestalHistoBinPacket;

/// \brief Prints a packet according to its type (DCC data, pedestal histogram statistics, bins or
/// summary, ADC data)
void DataPacket_Print(DataPacket* pck, FILE* fp = stdout);
/// \brief Prints a DCC data packet (end of event)
void DCC_Data_Print(EndOfEventPacket* pck, FILE* fp = stdout);
/// \brief Prints a DCC histogram packet
void DCC_Histogram_Print(HistogramPacket* pck, FILE* fp = stdout);
/// \brief Prints an end of event packet
void EndOfEvent_PrintPacket(EndOfEventPacket* eop, FILE* fp = stdout);
/// \brief Prints the statistics of a pedestal histogram
void Pedestal_PrintHistoMathPacket(PedestalHistoMathPacket* phm, FILE* fp = stdout);
/// \brief Prints the bins of a pedestal histogram
void Pedestal_PrintHistoBinPacket(PedestalHistoBinPacket* pck, FILE* fp = stdout);
/// \brief Prints the pedestal summary of an ASIC
void Pedestal_PrintHistoSummaryPacket(PedestalHistoSummaryPacket* pck, FILE* fp = stdout);
/// \brief Prints a packet of ADC data (headers and samples)
void FemAdcDataPrint(DataPacket* pck, FILE* fp = stdout);
/// \brief FEC, ASIC and channel of the read-back arguments of a packet
/// \return the physical channel, fec * 288 + asic * 72 + signal channel (the channels 0-2, 15, 28, 53
/// and 66, without signal, are skipped), or a negative value for a channel without signal
int Arg12ToFecAsicChannel(unsigned short arg1, unsigned short arg2, unsigned short& fec, unsigned short& asic,
                          unsigned short& channel);
/// \brief As Arg12ToFecAsicChannel, with the channel given
int Arg12ToFecAsic(unsigned short arg1, unsigned short arg2, unsigned short& fec, unsigned short& asic,
                   unsigned short channel);
/// \brief Looks for the end-of-event mark (0xFFFF and the 3 words of the timestamp, added by
/// FEMDAQDCC) in the buffer of data packets, from idx
/// \param buffer data packets of the DCC; the words of the event are removed if it is complete
/// \param idx position from which to search; the position reached if incomplete, 0 if complete
/// \param out the words of the event, if complete
/// \return whether a complete event was extracted
bool TryExtractNextEvent(std::deque<uint16_t>& buffer, size_t& idx, std::vector<uint16_t>& out);
/// \brief Adds a signal of 512 samples per data packet to sEvent (ID from Arg12ToFecAsicChannel; empty
/// packets and channels without signal are skipped)
/// \param tS timestamp of the end-of-event mark (48 bits, ticks of 10 ns)
/// \param ev_count event counter of the packets (14 bits)
void ParseEventFromWords(std::vector<uint16_t>& event, TRestRawSignalEvent& sEvent, uint64_t& tS,
                         uint32_t& ev_count);
}  // namespace DCCPacket
