The following example has been extracted from the pipeline validation at the main framework, where
the full data processing Montecarlo chain is validated.

We use Xe136bb0n.yaml to run restG4 for Geant4 simulation. The test of this example
(`packages/restG4/test/src/examples.cxx`, ctest with BUILD_TESTING) checks that the simulation goes smoothly,
i.e. all the events are stored and the metadata is correctly recorded. The output file will
be used later on
