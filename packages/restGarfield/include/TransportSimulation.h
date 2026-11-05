#ifndef RESTGARFIELD_TRANSPORTSIMULATION_H
#define RESTGARFIELD_TRANSPORTSIMULATION_H

#include <memory>

namespace Garfield {
class MediumMagboltz;
}
class TRestDetectorMedium;
class TRestDetectorTransportTable;

/// \brief Garfield++ gas (MediumMagboltz) of a gas medium at a pressure (bar) and temperature (K), without
/// gas table. Throws if the medium is not a gas or its components are not valid for Garfield.
std::unique_ptr<Garfield::MediumMagboltz> CreateGas(const TRestDetectorMedium& medium, double pressure,
                                                    double temperature);

/// \brief Computes the tables of a TRestDetectorTransportTable configuration with Garfield++: for each
/// condition, creates the Garfield medium of the type of the medium definition (MediumMagboltz, which
/// computes the gas table with Magboltz, MediumSilicon, ...), evaluates the transport parameters on the field
/// grid and adds the table to the medium file (TRestDetectorTransportTable).
void RunTransportSimulation(const TRestDetectorMedium& medium, TRestDetectorTransportTable& simulation,
                            bool verbose);

#endif
