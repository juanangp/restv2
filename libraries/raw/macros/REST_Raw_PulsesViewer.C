#include <string>

#include "TRestRawPulsesViewer.h"

/// \brief Opens the event viewer of raw signal files (TRestRawPulsesViewer, formerly DrawPulses of FEMDAQ):
/// the signals of each event, a pad per ASIC. With the file `fileName` if given (otherwise "Open File").
///
/// \param fileName REST file with the tree TRestRawSignalEvent (empty: choose it with the Open File button).
///
/// Usage in restRoot:
/// \code
/// restRoot
/// [0] REST_Raw_PulsesViewer("RawSignals.root")
/// \endcode
void REST_Raw_PulsesViewer(const std::string& fileName = "") {
    auto viewer = new TRestRawPulsesViewer();
    if (!fileName.empty()) viewer->LoadFile(fileName.c_str());
}
