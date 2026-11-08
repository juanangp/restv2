#include <string>

#include "TRestDAQViewer.h"

/// \brief Opens the online event display of raw signal files (TRestDAQViewer), e.g. those of femclient.
///
/// The window shows, for each readout, the amplitude spectrum, the hit map and the signals of the event,
/// plus the rate; with "DAQ Live Mode" it follows the file while it is written.
///
/// \param configFile A YAML of restv2 with the TRestDetectorReadoutManager and the
/// TRestRawSignalAnalysisProcess (e.g. the one of the analysis, or libraries/connectors/examples/
/// DAQViewer.yaml). Empty: the one of the last session.
/// \param dataFile The REST file with a TRestRawSignalEvent tree. Empty: the one of the last session.
///
/// Usage in restRoot:
/// \code
/// restRoot
/// [0] REST_DAQViewer("libraries/connectors/examples/DAQViewer.yaml", "Run00012_..._000.root")
/// \endcode
void REST_DAQViewer(const std::string& configFile = "", const std::string& dataFile = "") {
    new TRestDAQViewer(configFile, dataFile);
}
