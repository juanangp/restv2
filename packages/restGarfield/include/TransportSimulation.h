#ifndef RESTGARFIELD_TRANSPORTSIMULATION_H
#define RESTGARFIELD_TRANSPORTSIMULATION_H

#include <memory>

/// \file TransportSimulation.h
/// \brief Transport tables of restGarfield (drift velocity, diffusion, Townsend and attachment
/// coefficients), computed with Garfield++ (Magboltz for gases).

namespace Garfield {
class MediumMagboltz;
}
class TRestDetectorMedium;
class TRestDetectorTransportTable;

/// \brief Garfield++ gas (MediumMagboltz) of a gas medium at a pressure (bar) and temperature (K), without
/// gas table. Throws if the medium is not a gas or its components are not valid for Garfield.
/// \param medium Gas medium definition (1 to 6 components, by name and fraction).
/// \param pressure Pressure in bar.
/// \param temperature Temperature in K.
/// \return The Garfield gas, owned by the caller.
std::unique_ptr<Garfield::MediumMagboltz> CreateGas(const TRestDetectorMedium& medium, double pressure,
                                                    double temperature);

/// \brief Computes the tables of a TRestDetectorTransportTable configuration with Garfield++: for each
/// condition, creates the Garfield medium of the type of the medium definition (MediumMagboltz, which
/// computes the gas table with Magboltz, MediumSilicon, ...), evaluates the transport parameters on the field
/// grid and adds the table to the medium file (TRestDetectorTransportTable).
///
/// One table per temperature (by default the one of the medium) at the pressure of the configuration. Only
/// the carriers (electron, hole, ion) that the Garfield medium provides are stored; electrons are required.
/// A table already in the medium file is skipped unless overwrite is set.
///
/// \param medium Medium definition: a gas, or a solid of a single component Si, GaAs, GaN, CdTe or diamond.
/// Its mediumFile receives the tables.
/// \param simulation Configuration of the transport tables (the `transport` section of
/// TRestDetectorTransportManager).
/// \param verbose Verbose Magboltz gas table generation (GenerateGasTable).
/// \throws std::runtime_error if the medium is not supported, usePenning is set for a non-gas, there is no
/// electron transport or the medium file cannot be opened.
void RunTransportSimulation(const TRestDetectorMedium& medium, TRestDetectorTransportTable& simulation,
                            bool verbose);

#endif
