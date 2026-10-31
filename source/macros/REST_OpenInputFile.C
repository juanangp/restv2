
#include "TRestDataSet.h"
#include "TRestEvent.h"
#include "TRestRun.h"
#include "TRestTools.h"

TRestRun* _run = nullptr;
TRestEvent* _ev = nullptr;
TRestDataSet* _dataSet = nullptr;

/// Opens a dataset file as `_dataSet` (TRestDataSet(fileName): as it is, it is not generated again)
int REST_OpenDataSet(const std::string& filename) {
    delete _dataSet;
    _dataSet = nullptr;
    try {
        _dataSet = new TRestDataSet(filename);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
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

/// Opens a run file as `_run`, with its first event as `_ev`
int REST_OpenRun(const std::string& filename) {
    _run = new TRestRun(filename);

    if (!_run->fInputFile || !_run->fInputFile->IsOpen()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        delete _run;
        _run = nullptr;
        return -1;
    }

    _run->GetEntry(0);

    _ev = _run->GetEvent();

    if (!_ev) {
        std::cerr << "Error: Failed to retrieve a valid TRestEvent from the file." << std::endl;
        return -1;
    }

    std::cout << "Successfully loaded TRestRun. Event type detected: " << _ev->GetClassName() << std::endl;
    std::cout << "--> Global pointers '_run' and '_ev' are now fully accessible in your prompt!\n"
              << std::endl;

    return 0;
}

/// Opens a dataset (REST_OpenDataSet) or a run file (REST_OpenRun)
int REST_OpenInputFile(const std::string& filename) {
    std::cout << "--- Opening REST file: " << filename << " ---" << std::endl;

    if (TRestTools::isDataSet(filename)) return REST_OpenDataSet(filename);
    if (TRestTools::isValidTRestRun(filename)) return REST_OpenRun(filename);

    std::cerr << "Error: " << filename << " is not a REST run file nor a dataset" << std::endl;
    return -1;
}
