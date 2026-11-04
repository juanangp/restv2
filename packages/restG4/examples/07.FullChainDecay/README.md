**Date:** 12/March/2021

**Author:** Javier Galan (javier.galan@unizar.es)

This example tests a radiative chain of a full decay chain of U238. Its daughters, and the correct separation into subevents.

#### Detector geometry

This example implements a simple geometry based on a box made of lead.

#### Event generator
The event generator will produce isotope decays from the center of the box. The parent isotope is the parameter `isotope` of the YAML files, U238 by default (`--set isotope=Pb210`).

#### Testing the example
Execute the following to launch a full chain decay till the chain reaches the stable element.

```
restG4 fullChain.yaml
```

Execute the following to launch a single decay of the isotope (parameter `isotope`)

```
restG4 singleDecay.yaml
```

See also the test of this example in `packages/restG4/test/src/examples.cxx` (ctest, BUILD_TESTING).
