
#include "TRestRun.h"
#include "TRestEvent.h"


TRestRun* _run = nullptr;
TRestEvent* _ev = nullptr;

int REST_OpenInputFile(const std::string &filename){

    std::cout << "--- Opening REST file: " << filename << " ---" << std::endl;

    _run = new TRestRun(filename);

    if (!_run->fInputFile->IsOpen()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        delete _run;
        _run = nullptr;
        return -1;
    }

    _run->GetEntry(0);

    _ev = _run->GetInputEvent();

    if (!_ev) {
        std::cerr << "Error: Failed to retrieve a valid TRestEvent from the file." << std::endl;
        return -1;
    }

    std::cout << "Successfully loaded TRestRun. Event type detected: " << _ev->GetClassName() << std::endl;
    std::cout << "--> Global pointers '_run' and '_ev' are now fully accessible in your prompt!\n" << std::endl;

  return 0;
}


