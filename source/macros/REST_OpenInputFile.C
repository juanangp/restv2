
#include "TRestDataSet.h"
#include "TRestEvent.h"
#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

/// \file REST_OpenInputFile.C
/// \brief Opens a REST run file or dataset in restRoot and exposes it through global pointers.
///
/// restRoot calls REST_OpenInputFile for the REST file given in its command line (`restRoot file.root`).

/// Run opened by REST_OpenRun
TRestRun* _run = nullptr;
/// Event of `_run` (its first entry after REST_OpenRun)
TRestEvent* _ev = nullptr;
/// Dataset opened by REST_OpenDataSet
TRestDataSet* _dataSet = nullptr;

/// \brief Opens a dataset file as `_dataSet` (TRestDataSet(fileName): as it is, it is not generated again)
///
/// The previous `_dataSet` is deleted. Prints examples of use of `_dataSet`.
///
/// \param filename Dataset ROOT file.
/// \return 0 on success, -1 if the dataset cannot be opened (`_dataSet` is then null).
///
///    restRoot
///    REST_OpenDataSet("D_Calibration.root")
int REST_OpenDataSet(const std::string& filename) {
    delete _dataSet;
    _dataSet = nullptr;
    try {
        _dataSet = new TRestDataSet(filename);
    } catch (const std::exception& error) {
        RESTError << "REST_OpenDataSet: " << error.what() << RESTendl;
        return -1;
    }

    std::cout << "Successfully loaded TRestDataSet with " << _dataSet->GetRuns().size() << " runs."
              << std::endl;
    std::cout << "--> Global pointer '_dataSet' is now accessible in your prompt, e.g.:" << std::endl;
    std::cout << "    _dataSet->PrintDataSetInfo()" << std::endl;
    std::cout << "    _dataSet->PrintAllMetadata()" << std::endl;
    std::cout << "    _dataSet->GetRuns()[0].run->GetRunTag()" << std::endl;
    std::cout << "    _dataSet->GetDataFrame().GetColumnNames()" << std::endl;
    std::cout << "    _dataSet->GetDataFrame().Display({\"runIndex\", \"eventID\"})->Print()" << std::endl;
    std::cout << "    _dataSet->GetRunParameter(\"runTag\")\n" << std::endl;
    return 0;
}

/// \brief Opens a run file as `_run`, with its first event as `_ev`
///
/// The previous `_run` is deleted (and `_ev`, owned by it, reset). On failure both are left null.
///
/// \param filename REST run file.
/// \return 0 on success, -1 if the file cannot be opened or has no valid event (`_run` and `_ev` are then
/// null).
///
///    restRoot
///    REST_OpenRun("R01234_ANA.root")
///    _ev->PrintEvent()
int REST_OpenRun(const std::string& filename) {
    _ev = nullptr;
    delete _run;
    _run = nullptr;

    TRestRun* run = nullptr;
    try {
        run = new TRestRun(filename);

        if (!run->fInputFile || !run->fInputFile->IsOpen()) {
            RESTError << "REST_OpenRun: could not open file " << filename << RESTendl;
            delete run;
            return -1;
        }

        run->GetEntry(0);
    } catch (const std::exception& error) {
        RESTError << "REST_OpenRun: " << error.what() << RESTendl;
        delete run;
        return -1;
    }

    TRestEvent* event = run->GetEvent();
    if (!event) {
        RESTError << "REST_OpenRun: failed to retrieve a valid TRestEvent from the file." << RESTendl;
        delete run;
        return -1;
    }

    _run = run;
    _ev = event;

    std::cout << "Successfully loaded TRestRun. Event type detected: " << _ev->GetClassName() << std::endl;
    std::cout << "--> Global pointers '_run' and '_ev' are now fully accessible in your prompt!\n"
              << std::endl;

    return 0;
}

/// \brief Opens a dataset (REST_OpenDataSet) or a run file (REST_OpenRun)
///
/// \param filename REST file: a dataset (TRestTools::isDataSet) or a run file (TRestTools::isValidTRestRun).
/// \return The result of REST_OpenDataSet or REST_OpenRun, -1 if the file is neither of them.
///
///    restRoot
///    REST_OpenInputFile("R01234_ANA.root")
int REST_OpenInputFile(const std::string& filename) {
    std::cout << "--- Opening REST file: " << filename << " ---" << std::endl;

    if (TRestTools::isDataSet(filename)) return REST_OpenDataSet(filename);
    if (TRestTools::isValidTRestRun(filename)) return REST_OpenRun(filename);

    RESTError << "REST_OpenInputFile: " << filename << " is not a REST run file nor a dataset" << RESTendl;
    return -1;
}
