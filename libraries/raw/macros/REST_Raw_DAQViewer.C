#include <string>

#include "TRestRawDAQViewer.h"

/// \brief Opens the online event display of raw signal files (TRestRawDAQViewer), e.g. the files written by
/// restDAQ, with the file `fileName` if given (otherwise the last one opened, or "Open Data").
void REST_Raw_DAQViewer(const std::string& fileName = "") {
    auto viewer = new TRestRawDAQViewer();
    if (!fileName.empty()) viewer->LoadDataFile(fileName.c_str());
}
