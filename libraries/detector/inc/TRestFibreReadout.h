#ifndef TRESTFIBREREADOUT_H
#define TRESTFIBREREADOUT_H

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "TRestDetectorReadout.h"

/// \class TRestFibreFamily
/// \brief A family of parallel fibres of a TRestFibreReadout (an entry of `fibres`): their axis, the grid
/// of their positions in the plane transverse to it and their capture probability.
class TRestFibreFamily : public TRestMetadata {
   public:
    /// `axis`: direction of the fibres, "X", "Y" or "Z"
    std::string fAxis = "Z";
    /// `pitch`: distance between neighbour fibres, the same along both transverse axes
    TRestWithUnits fPitch = 0;
    /// `offset`: position of the first fibre along the two transverse axes (in cyclic order after the axis:
    /// X: Y and Z; Y: Z and X; Z: X and Y), from the lower corner of the box of the readout
    std::array<TRestWithUnits, 2> fOffset = {0.0, 0.0};
    /// `captureProbability`: probability that a scintillation photon produced at a distance r (mm) from a
    /// fibre is captured and reaches its photosensor, as a ROOT formula of x = r (e.g.
    /// "0.05*exp(-x*x/200)") or a graph or histogram of a ROOT file, "file.root:name" (e.g. from an optical
    /// simulation of Geant4)
    std::string fCaptureProbability = "";
    /// `maxDistance`: fibres farther than this from a position collect no light from it (the range of
    /// captureProbability)
    TRestWithUnits fMaxDistance = 0;
    /// `volume`: logical volume of the fibre in the GDML of the simulation (the placements written by
    /// TRestFibreReadout::WriteGDMLPlacements)
    std::string fVolume = "";

    /// Capture probability versus the distance (mm), built from captureProbability
    std::function<double(double)> fCapture;  //!

    /// \brief Builds an empty family.
    TRestFibreFamily();
    /// \brief Builds the family from its YAML node (LoadConfig).
    TRestFibreFamily(const std::string& name, const YAML::Node& node);
    /// \brief Reads the keys and builds the capture probability; throws if the axis is not X, Y or Z, the
    /// pitch or maxDistance are not positive or captureProbability is not a valid formula or graph.
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Returns the class name, "TRestFibreFamily".
    std::string GetClassName() const override { return "TRestFibreFamily"; }

    /// \brief Axis of the fibres: 0 X, 1 Y, 2 Z.
    int GetAxis() const { return fAxis == "X" ? 0 : fAxis == "Y" ? 1 : 2; }
    /// \brief Capture probability at a distance (mm) from a fibre (0 beyond maxDistance).
    double GetCaptureProbability(double distance) const;
};

/// \class TRestFibreReadoutParameters
/// \brief The `readoutParameters` of a TRestFibreReadout: the box (size, position), the quantum
/// efficiency of the photosensors and the fibre families (`fibres`).
class TRestFibreReadoutParameters : public TRestMetadata {
   public:
    /// `size`: size of the box filled with fibres (the scintillator), along X, Y and Z
    std::array<TRestWithUnits, 3> fSize = {0.0, 0.0, 0.0};
    /// `position`: centre of the box in the global frame (the placement of the scintillator volume, not
    /// rotated)
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};
    /// `quantumEfficiency`: probability that a photon reaching the photosensor (SiPM) of a fibre gives a
    /// photoelectron
    double fQuantumEfficiency = 1;
    /// `fibres`: the fibre families
    std::vector<TRestFibreFamily> fFibres;

    /// \brief Builds empty parameters.
    TRestFibreReadoutParameters();
    /// \brief Builds the parameters from the `readoutParameters` node (LoadConfig).
    TRestFibreReadoutParameters(const std::string& name, const YAML::Node& node);
    /// \brief Reads the parameters; throws if the size is not positive, the quantum efficiency is not in
    /// [0, 1] or there are no fibres.
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Returns the class name, "TRestFibreReadoutParameters".
    std::string GetClassName() const override { return "TRestFibreReadoutParameters"; }
};

/// \class TRestFibreReadout
/// \brief Readout of a scintillator crossed by a lattice of fibres, each one read by a photosensor (e.g.
/// LiquidO, an opaque scintillator with wavelength shifting fibres read by SiPMs).
///
/// The parameters and an example are described in the class documentation (TRestFibreReadout.cxx).
class TRestFibreReadout : public TRestDetectorReadout {
   public:
    TRestFibreReadoutParameters fReadoutParams;  ///< `readoutParameters`

   private:
    /// The grid of a family: axis, transverse axes, number of fibres along them and first physical ID
    struct Grid {
        int axis = 2;
        int u = 0;
        int v = 1;
        int nU = 0;
        int nV = 0;
        int firstID = 0;
    };
    std::vector<Grid> fGrids;  //!

    /// \brief Builds the grids of the families from the parameters (LoadConfig).
    void BuildGrids();
    /// \brief Lower corner of the box.
    double GetMinimum(int axis) const {
        return fReadoutParams.fPosition[axis] - fReadoutParams.fSize[axis] / 2;
    }
    /// \brief Transverse coordinate of fibre index along the transverse axis t (0: u, 1: v) of a family.
    double GetFibreCoordinate(size_t family, int t, int index) const;
    /// \brief Family and indices of a physical ID; false if it is not a fibre.
    bool GetFibre(int physicalID, size_t& family, int& i, int& j) const;
    /// \brief DAQ ID of a physical ID, -1 if not connected.
    int GetDAQID(int physicalID) const;

   public:
    /// \brief Constructor.
    TRestFibreReadout();
    /// \brief Builds a fibre readout from an in-memory YAML node section.
    TRestFibreReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds a fibre readout from file+section metadata references.
    TRestFibreReadout(const std::string& fileName, const std::string& sectionName);

    /// \brief Returns the class name, "TRestFibreReadout".
    std::string GetClassName() const override { return "TRestFibreReadout"; }
    /// \brief Loads the parameters and builds the grids of the families.
    void LoadConfig() override;

    /// \brief Builds the geometry (the fibres as thin tubes, to draw them) and the decoding: the decoding
    /// file if `decodingFile` is given, otherwise the DAQ ID of each fibre is its physical ID.
    void BuildGeometry() override;

    /// \brief Total number of fibres.
    int GetNumberOfFibres() const;
    /// \brief The closest fibre (any family) to a position inside the box, measured in the plane transverse
    /// to the fibre; -1 outside the box or if it is not connected.
    int GetChannelFromPosition(double x, double y, double z) const override;
    /// \brief Position of a fibre: its two transverse coordinates, the coordinate along it NaN (e.g. a
    /// fibre along Y gives an XZ hit).
    ROOT::Math::XYZVector GetPositionFromChannel(int daqID) const override;
    /// \brief The box of the readout.
    std::pair<ROOT::Math::XYZVector, ROOT::Math::XYZVector> GetChannelsBoundingBox() const override;
    /// \brief Neighbour fibres of the same family and row (physical IDs - 1 and + 1 along the second
    /// transverse axis), -1 at the ends.
    std::pair<int, int> GetAdjacentChannels(int daqID) const override;
    /// \brief The fibres closer than the maxDistance of their family to the position (the coordinate along
    /// the fibre inside the box), with probability captureProbability(r) * quantumEfficiency, at the
    /// projection of the position on the fibre.
    std::vector<LightCollection> GetLightCollection(const ROOT::Math::XYZVector& position) const override;

    /// \brief Writes the placements of the fibres (`physvol` elements of the GDML, named fibre_<physical
    /// ID>, with the logical volume `volume` of their family) to a file, to be included as an external
    /// entity inside the volume of the scintillator (placed at `position`, not rotated).
    /// \return false if the file cannot be written or a family has no volume.
    bool WriteGDMLPlacements(const std::string& fileName) const;
};

#endif
