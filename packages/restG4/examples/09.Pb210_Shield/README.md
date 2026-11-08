**Date:** 30/April/2021

**Author:** Javier Galan (javier.galan@unizar.es)


#### Detector geometry

This example implements a small empty lead box, size 10x10x10cm3 and 2mm thickness, which covers a detector of size 6x6x3cm3 filled with xenon at 1bar.

#### Event generator
The event generator will produce Pb210 isotope decays from the lead volume.

#### Testing the example
This example will generate the full Pb210 chain and it will register only the events that produce events in the (0,10)keV range inside the Xenon detection volume.

```
restG4 Pb210.yaml
```

See also the test of this example in `packages/restG4/test/src/examples.cxx` (ctest, BUILD_TESTING).

#### Volume biasing

`Biasing.yaml` simulates 1 MeV gammas from a wall above the lead box with two nested virtual biasing volumes (a
sphere and a box around the lead box, factor 10 each): only the events of the last stage are written, each one
with weight 1/100 (see the volume biasing section of the restG4 README).

```
restG4 Biasing.yaml
```
