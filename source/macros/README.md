# REST macros (source/macros)

The macros of the framework. Each `.C` file is compiled by CMake into a library (`lib<macro>_C.so`, installed
in `$REST_PATH/macros`) and its `REST_*` functions and global pointers are declared in the generated
`REST_Macros.h`. `restRoot` loads both at startup, so the functions can be called directly from its prompt
(do not `.L`/`.x` the installed `.C` files: they would redefine the declared functions).

| Macro | Description | Call |
|---|---|---|
| `REST_OpenInputFile` | Opens a dataset (as `_dataSet`) or a run file (as `_run`, first event `_ev`); restRoot calls it for a REST file given on its command line. | `REST_OpenInputFile("R01234_ANA.root")` |
| `REST_OpenRun` | Opens a run file as `_run`, with its first event as `_ev` (in `REST_OpenInputFile.C`); the previous `_run` is deleted, and on failure `_run` and `_ev` are left null. | `REST_OpenRun("R01234_ANA.root")` |
| `REST_OpenDataSet` | Opens a dataset file as `_dataSet`, as it is (in `REST_OpenInputFile.C`). | `REST_OpenDataSet("D_Calibration.root")` |
| `REST_GenerateDataSet` | Generates the TRestDataSet of a YAML section with its cuts and writes it. | `REST_GenerateDataSet("dataset.yaml", "dataSet" [, "output.root", "inputPattern"])` |
| `REST_MergeRuns` | Merges the subruns of a run (or simulations of the same configuration with different seeds) into a single run file. | `REST_MergeRuns("R01234_*_ANA.root", "R01234_ANA.root")` |
| `REST_ViewTMVA` | Draws the training results of a TRestDataSetTMVA stored in a dataset (ROC curve, training/test scores, separation and importance of the observables), optionally saved. | `REST_ViewTMVA("Dataset_Background_TMVA.root", "bdt" [, "bdt.pdf"])` |
| `REST_ViewEvents` | Opens a run file in the event browser (TRestBrowser + TRestEventViewer). | `REST_ViewEvents("R01234_ANA.root" [, "TRestDetectorHitsEvent"])` |

All of them return 0 on success and a non-zero value on error (the reason is printed).

Example:

```
restRoot
root [0] REST_MergeRuns("R01234_*_ANA.root", "R01234_ANA.root")
```

Macros of the libraries (`libraries/*/macros`) and of the packages (e.g. `packages/restGarfield/macros`) are
compiled and loaded the same way.
