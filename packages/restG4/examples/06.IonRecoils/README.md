**Date:** 10/March/2021

**Author:** Javier Galan (javier.galan@unizar.es)

This example tests the customization of integration step limits for ions through the parameter `ionLimitStepList`, where we will be able to define the ions for which the step limitation defined at each `activeVolume` using the `maxStepSize` parameter.

The parameter defining the isotopes affected by the `maxStepSize` is included at the `TRestGeant4PhysicsLists` section, where any number of ions to be tracked can be given as a list.

```
    <parameter name="ionLimitStepList" value="F20,Ne20" />
```

#### Detector geometry

This example implements a simple geometry based on a box full of a Neon gas mixture.

#### Event generator
The event generator will launch isotope recoils from the center of the box in isotropic directions. The ion is the parameter `isotope` of recoils.yaml, F20 by default (`restG4 recoils.yaml --set isotope=Ne20`).

#### Testing the example
Execute the following to launch the recoils

```
restG4 recoils.yaml
```

See also the test of this example in `packages/restG4/test/src/examples.cxx` (ctest, BUILD_TESTING).
