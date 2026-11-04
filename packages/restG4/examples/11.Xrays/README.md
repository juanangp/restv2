**Date:** 14/March/2022

**Author:** Javier Galan (javier.galan@unizar.es)

This example uses an x-ray gun with photons in the (0,15)keV range hitting a gas volume filled with Xenon in order to generate an efficiency response matrix.

#### Detector geometry

This example implements a simple geometry based on a cylinder full of a Xenon gas mixture.

#### Testing the example

There are two independent examples that exploit this detector geometry

##### Example 1. Launching gammas

Execute the following to launch the x-rays

```
restG4 xrays.yaml
restManager --c analysis.yaml --i RestG4_xrays_00001_ArgonISO.root
```

The efficiency is the fraction of the gammas with energy in the gas (`g4Ana_totalEdep > 0`) as a function
of `g4Ana_energyPrimary` (e.g. REST_Geant4_GenerateResponseMatrix or a TRestAnalysisPlot).

##### Example 2. Fe55 isotropic source generator

Execute the following to launch the Fe55 source events

```
restG4 Fe55.yaml -o RestG4_Fe55_ArgonISO.root
restManager --c analysis.yaml --i RestG4_Fe55_ArgonISO.root
```

The test of this example (`packages/restG4/test/src/examples.cxx`) verifies that the number of events inside the peak remains correct.
