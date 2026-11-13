# Examples

High-level analysis of datasets (TRestDataSetProcessManager), not tied to a library. Their inputs are the
calibrated datasets of `libraries/detector/examples/DataSetCalibration.yaml` (`Dataset_*_Calibrated.root`),
except where noted.

| File | Contents |
|---|---|
| `FemDAQToTracks.yaml` | Full chain of the IAXO D1 data of the old FEMDAQ, from raw signals to track analysis (readout and medium files relative to the repository root). |
| `DataSetLogOdds.yaml` | Log odds of observables (`TRestDataSetLogOdds`), PDFs from the calibration. |
| `DataSetTMVA.yaml` | BDT of TMVA (`TRestDataSetTMVA`), the calibration as signal and a background dataset. |
| `DataSetSignalSelection.yaml` | Selection (`TRestDataSetSignalSelection`) of the entries with the BDT score of the 80% of the calibration. |
| `DataSetCombine.yaml` | Combination (`TRestDataSetCombine`) of the datasets of the Micromegas and of the vetoes, analysed apart, matched by event. |
| `DataSetTardis.yaml` | Chronological time differences (`TRestDataSetTardis`) for any dataset with both timestamp columns. |
| `TREXDMToTracks.yaml` | Full chain of the TREX-DM FEMDAQ data of one Micromegas plane (`--set plane=0` or `1`), from raw signals to track analysis (Ne + 2% isobutane at 2 bar), with the physical channels of each event (`rawToSignal_SignalsPhysicalID`); output named as the input. |
| `TREXDMTardis.yaml` | Dataset of a TREX-DM plane processed by `TRestDataSetTardis`: time since the previous event in the same strips (±1 strip in X and in Y), the noise removed by a cut, with the track observables. |
| `TREXDMTardisPlots.yaml` | Plots of the dataset of `TREXDMTardis.yaml`: the delays and the energy of the tracks of the correlated events. |

```
restManager --c examples/FemDAQToTracks.yaml --i Run00012_Calibration_IAXO_Data_000.root
restManager --c examples/DataSetLogOdds.yaml --i Dataset_Background_Calibrated.root
restManager --c examples/DataSetTMVA.yaml --i Dataset_Background_Calibrated.root
restManager --c examples/DataSetSignalSelection.yaml --i Dataset_Background_TMVA.root
restManager --c examples/DataSetTardis.yaml --i Dataset.root --o Dataset_Tardis.root
restManager --c examples/TREXDMToTracks.yaml --i Run03318_Calibration109Cd_TREXDM_DAQ_001.root --set plane=0
restManager --c examples/TREXDMTardis.yaml --i "Run*_TREXDM_Plane0_*.root"
restManager --c examples/TREXDMTardisPlots.yaml --i Dataset_03318_Calibration109Cd_TREXDM_Plane0_Tardis.root
```
