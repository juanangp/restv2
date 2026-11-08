#include <string>

#include "TRestDAQViewer.h"

/// \brief Opens the online event display of raw signal files (TRestDAQViewer), e.g. those of femclient.
/// configFile: a YAML of restv2 with the TRestDetectorReadoutManager and the TRestRawSignalAnalysisProcess
/// (e.g. the one of the analysis); dataFile: the REST file. Empty: those of the last session.
void REST_DAQViewer(const std::string& configFile = "", const std::string& dataFile = "") {
    new TRestDAQViewer(configFile, dataFile);
}
