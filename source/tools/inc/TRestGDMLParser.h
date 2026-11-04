#ifndef TRESTGDMLPARSER_H
#define TRESTGDMLPARSER_H

#include <map>
#include <set>
#include <string>

class TGeoManager;

/// GDML geometries as REST reads them (restG4, TRestGeant4GeometryInfo, TRestGDMLReadout,
/// REST_Geant4_ViewGeometry): with the entities expanded and the parameters of the configuration
/// (TRestYAMLUtils::GetParameters) replacing the GDML constants and entities with their name, so a GDML
/// keeps its default values and stays valid for any viewer (no environment variables in it).
namespace TRestGDMLParser {

/// \brief GDML with its entities and parameters resolved, in a single text without dependencies:
/// * the entities of the DOCTYPE (`<!ENTITY name SYSTEM "file">` and `<!ENTITY name "text">`, also those of
///   the files included) replace their references (&name;), recursively: the files relative to the one that
///   includes them, the URLs (http, https) downloaded (TRestTools::DownloadFile); the DOCTYPE is removed;
/// * a parameter (`parameters`, name -> value) with the name of a text entity replaces its text, and one with
///   the name of a `<constant>` or `<variable>` its value: a number in REST units (e.g. "5 um" as 0.005, mm;
///   "1.5 bar" as 1.5), an error if it is not a number.
/// The names of the parameters applied are added to `applied` if given. The GDML keeps its default values
/// for the parameters not given, so it is a valid GDML that any viewer opens.
std::string Resolve(const std::string& gdmlFile, const std::map<std::string, std::string>& parameters,
                    std::set<std::string>* applied = nullptr);
/// \brief Resolve with the parameters of the configuration (TRestYAMLUtils::GetParameters, marked as
/// used) written to a temporary file (system temporary directory): its path, to be removed by the caller
std::string WriteResolved(const std::string& gdmlFile);
/// \brief TGeoManager::Import of the resolved GDML (WriteResolved, the temporary file removed)
TGeoManager* Import(const std::string& gdmlFile);
/// \brief Writes the parameters in the GDML files (restG4 --update-gdml): the value of the constants,
/// variables and text entities with the name of a parameter, in the GDML and in the local files it includes
/// (not in the URLs), without changing anything else. The names of the parameters written are added to
/// `applied` if given.
void UpdateParameters(const std::string& gdmlFile, const std::map<std::string, std::string>& parameters,
                      std::set<std::string>* applied = nullptr);

}  // namespace TRestGDMLParser

#endif
