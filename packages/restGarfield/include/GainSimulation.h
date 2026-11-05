#ifndef RESTGARFIELD_GAINSIMULATION_H
#define RESTGARFIELD_GAINSIMULATION_H

class TRestDetectorMedium;
class TRestDetectorGainTable;

/// \brief Computes the tables of a TRestDetectorGainTable configuration with Garfield++: for each condition
/// and field node, simulates microscopic avalanches (AvalancheMicroscopic) in a uniform field across the gap
/// (parallel plates), each one started by one electron at the mesh side, and adds the table (mean gain, Polya
/// theta from the moments and the distribution of the avalanche sizes) to the medium file. Only gases.
void RunGainSimulation(const TRestDetectorMedium& medium, TRestDetectorGainTable& simulation, bool verbose);

#endif
