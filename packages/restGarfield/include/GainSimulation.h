#ifndef RESTGARFIELD_GAINSIMULATION_H
#define RESTGARFIELD_GAINSIMULATION_H

/// \file GainSimulation.h
/// \brief Gain (amplification) tables of restGarfield, computed with Garfield++ microscopic avalanches.

class TRestDetectorMedium;
class TRestDetectorGainTable;

/// \brief Computes the tables of a TRestDetectorGainTable configuration with Garfield++: for each condition
/// and field node, simulates microscopic avalanches (AvalancheMicroscopic) in a uniform field across the gap
/// (parallel plates), each one started by one electron at the mesh side, and adds the table (mean gain, Polya
/// theta from the moments and the distribution of the avalanche sizes) to the medium file. Only gases.
///
/// The conditions are the pressures and temperatures of the configuration (by default those of the medium);
/// the fields are the reduced field nodes (E/p) times each pressure. A table already in the medium file is
/// skipped unless overwrite is set. With usePenning, the Penning transfer (penningRate) is enabled.
///
/// \param medium Medium definition (a gas); its mediumFile receives the tables.
/// \param simulation Configuration of the gain tables (the `gain` section of TRestDetectorTransportManager).
/// \param verbose Print the gain and theta of each field node.
/// \throws std::runtime_error if the medium is not a gas, its gas cannot be created, the Penning transfer
/// cannot be enabled or the medium file cannot be opened.
void RunGainSimulation(const TRestDetectorMedium& medium, TRestDetectorGainTable& simulation, bool verbose);

#endif
