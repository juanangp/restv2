# Examples

High-level analysis of datasets (TRestDataSetProcessManager), not tied to a library. Their inputs are the
calibrated datasets of `libraries/detector/examples/DataSetCalibration.yaml` (`Dataset_*_Calibrated.root`).

| File | Contents |
|---|---|
| `DataSetLogOdds.yaml` | Log odds of observables (`TRestDataSetLogOdds`), PDFs from the calibration. |
| `DataSetTMVA.yaml` | BDT of TMVA (`TRestDataSetTMVA`), the calibration as signal and a background dataset. |
| `DataSetSignalSelection.yaml` | Selection (`TRestDataSetSignalSelection`) of the entries with the BDT score of the 80% of the calibration. |

```
restManager --c examples/DataSetLogOdds.yaml --i Dataset_Background_Calibrated.root
restManager --c examples/DataSetTMVA.yaml --i Dataset_Background_Calibrated.root
restManager --c examples/DataSetSignalSelection.yaml --i Dataset_Background_TMVA.root
```
