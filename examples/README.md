# Examples

High-level analysis of datasets (TRestDataSetProcessManager), not tied to a library. Their inputs are the
calibrated datasets of `libraries/detector/examples/DataSetCalibration.yaml` (`Dataset_*_Calibrated.root`).

| File | Contents |
|---|---|
| `FemDAQToTracks.yaml` | Full chain of the IAXO D1 data of the old FEMDAQ, from raw signals to track analysis (readout and medium files relative to the repository root). |
| `DataSetLogOdds.yaml` | Log odds of observables (`TRestDataSetLogOdds`), PDFs from the calibration. |
| `DataSetTMVA.yaml` | BDT of TMVA (`TRestDataSetTMVA`), the calibration as signal and a background dataset. |
| `DataSetSignalSelection.yaml` | Selection (`TRestDataSetSignalSelection`) of the entries with the BDT score of the 80% of the calibration. |
| `DataSetCombine.yaml` | Combination (`TRestDataSetCombine`) of the datasets of the Micromegas and of the vetoes, analysed apart, matched by event. |

```
restManager --c examples/FemDAQToTracks.yaml --i Run00012_Calibration_IAXO_Data_000.root
restManager --c examples/DataSetLogOdds.yaml --i Dataset_Background_Calibrated.root
restManager --c examples/DataSetTMVA.yaml --i Dataset_Background_Calibrated.root
restManager --c examples/DataSetSignalSelection.yaml --i Dataset_Background_TMVA.root
```
