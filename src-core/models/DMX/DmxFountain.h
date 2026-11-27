#pragma once

/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "DmxModel.h"
#include "FountainPhysics.h"

#include <climits>
#include <cmath>
#include <functional>

class SequenceData;

// A water fountain: pumps laid out as a ring (optionally with one in the center), a line, around a square or in
// a grid (one DMX channel each, 0-255 = 0-100% of the max water height), each with an optional RGB light ring
// under it. The box is the pool: its width is the pool's diameter (or the line's length) and its height is the
// tallest pump's water at 100% (a center pump can have its own max height).
//
// Each light's red, green and blue channels take their own colour from an effect and share one cell (a pump has its
// own, beside it) in Single Line buffers; in Per Preview the pump and its light sit at the same spot, so a camera
// looking down paints them by where they are in the pool. Default stays one cell per channel for the DMX effect. A new fountain starts with Lights / Pumps submodels (and Perimeter / Center ones
// with a center pump); after that they are the user's to change. Previews fly the water with the same physics as the
// Fountain Studio web visualizer (FountainPhysics.h): air drag, the pump's spin-up, water that is already in the
// air flying on after a pump drops, and spray where falling water lands on rising water.
class DmxFountain : public DmxModel {
public:
    // In the order the property editors list them.
    enum class Arrangement {
        Ring = 0,
        RingWithCenter, // the last pump in the center, the others around it
        Line,
        Square,         // evenly spaced around the perimeter
        Grid,
        Count
    };

    static constexpr int MAX_PUMPS = 64;

    DmxFountain(const ModelManager& manager);
    virtual ~DmxFountain();

    void Accept(BaseObjectVisitor& visitor) const override { return visitor.Visit(*this); }

    [[nodiscard]] std::vector<std::string> GenerateNodeNames() const override;

    [[nodiscard]] const std::vector<std::string>& GetBufferStyles() const override;
    void GetBufferSize(const std::string& type, const std::string& camera, const std::string& transform,
                       int& BufferWi, int& BufferHi, int stagger) const override;
    void InitRenderBufferNodes(const std::string& type, const std::string& camera, const std::string& transform,
                               std::vector<NodeBaseClassPtr>& newNodes, int& BufferWi, int& BufferHi, int stagger, bool deep = false) const override;
    // A light's channels share a buffer cell (the group is its pump's number); a pump, or anything else, has its own.
    [[nodiscard]] int GetNodeBufferGroup(const NodeBaseClass& node) const override;
    // What a node of this fountain drives: 1 a pump, 2 a light (red, green or blue), 0 anything else.
    [[nodiscard]] int ChannelKind(const NodeBaseClass& node) const;
    // The pump (0-based) a node belongs to, as the pump itself or its light; -1 for anything else.
    [[nodiscard]] int ChannelPump(const NodeBaseClass& node) const;

    // The submodels a new fountain starts with: name and node ranges (1-based, a pump followed by its light).
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> GetDefaultSubModels() const;
    // Adds those of them it doesn't have (a new model gets them all).
    void CreateDefaultSubModels();
    void GetPWMOutputs(std::map<uint32_t, PWMOutput>& channels) const override;
    std::list<std::string> CheckModelSettings() override;

    [[nodiscard]] int GetPumpCount() const { return _pumpCount; }
    void SetPumpCount(int n) { _pumpCount = std::clamp(n, 1, MAX_PUMPS); }
    [[nodiscard]] Arrangement GetArrangement() const { return _arrangement; }
    void SetArrangement(Arrangement a) { _arrangement = a; }
    // Index into ArrangementLabels(); out of range gives a ring.
    static Arrangement ArrangementFromIndex(int i);
    [[nodiscard]] static const std::vector<std::string>& ArrangementLabels();
    [[nodiscard]] static std::string ArrangementToString(Arrangement a);
    [[nodiscard]] static Arrangement ArrangementFromString(const std::string& s);
    [[nodiscard]] bool IsClockwise() const { return _clockwise; }
    void SetClockwise(bool b) { _clockwise = b; }
    [[nodiscard]] int GetPumpChannel() const { return _pumpChannel; }
    void SetPumpChannel(int ch) { _pumpChannel = std::max(1, ch); }
    [[nodiscard]] bool HasLights() const { return _hasLights; }
    void SetHasLights(bool b) { _hasLights = b; }
    [[nodiscard]] int GetLightChannel() const { return _lightChannel; }
    void SetLightChannel(int ch) { _lightChannel = std::max(1, ch); }
    [[nodiscard]] float GetMaxHeightFt() const { return _maxHeightFt; }
    void SetMaxHeightFt(float ft) { _maxHeightFt = std::clamp(ft, 0.5f, 500.0f); }
    // A Ring with Center whose center pump has its own max height. Until one is set it follows Max Water Height;
    // setting it back to that value follows again.
    [[nodiscard]] bool HasCenterPump() const { return _arrangement == Arrangement::RingWithCenter && _pumpCount >= 2; }
    [[nodiscard]] float GetCenterMaxHeightFt() const { return _centerMaxHeightFt > 0 ? _centerMaxHeightFt : _maxHeightFt; }
    [[nodiscard]] bool IsCenterMaxHeightSet() const { return _centerMaxHeightFt > 0; }
    // The stored value: 0 while it follows Max Water Height.
    [[nodiscard]] float GetCenterMaxHeightSetting() const { return _centerMaxHeightFt; }
    void SetCenterMaxHeightFt(float ft) {
        _centerMaxHeightFt = ft <= 0 || std::fabs(ft - _maxHeightFt) < 0.005f ? 0.0f : std::clamp(ft, 0.5f, 500.0f);
    }
    // Max water height of one pump (the center pump's own when it has one), in feet.
    [[nodiscard]] float GetPumpMaxHeightFt(int pump) const {
        return HasCenterPump() && pump == _pumpCount - 1 ? GetCenterMaxHeightFt() : _maxHeightFt;
    }
    // How far out the nozzles sit, as a percentage of the pool (ring diameter, line length, square or grid side).
    [[nodiscard]] int GetNozzleSpread() const { return _nozzleSpread; }
    void SetNozzleSpread(int pct) { _nozzleSpread = std::clamp(pct, 0, 100); }
    [[nodiscard]] bool IsSprayEnabled() const { return _spray; }
    void SetSprayEnabled(bool b) { _spray = b; }
    [[nodiscard]] bool IsPoolShown() const { return _showPool; }
    void SetPoolShown(bool b) { _showPool = b; }
    [[nodiscard]] int GetDropletsPerPump() const { return _droplets; }
    void SetDropletsPerPump(int n) { _droplets = std::clamp(n, 50, 5000); }

    // The channels the pumps and lights need: the highest one used.
    [[nodiscard]] int GetRequiredChannelCount() const;
    // Lays the lights out right after the pumps and sizes the model to fit.
    void AssignSequentialChannels();
    // Rename the channels (Pump 1, Pump 1 Red, ...) on the next init, after the pumps or lights moved.
    void UpdateNodeNames() { _updateNodeNames = true; }

    // Pump positions across the pool (top view), as fractions of the box width: x right, z toward the viewer.
    // Pump 1 is at the front of a ring or square, numbered around as seen from above; a grid starts at the
    // front left, row by row toward the back.
    [[nodiscard]] std::vector<std::pair<float, float>> GetPumpLayout() const;

    // Pump i's level 0..1 and light color, read from one frame of channel values (absolute channels).
    [[nodiscard]] float PumpLevel(const unsigned char* frame, size_t numChannels, int pump) const;
    [[nodiscard]] xlColor PumpLight(const unsigned char* frame, size_t numChannels, int pump) const;

protected:
    virtual void InitModel() override;

    virtual void DisplayModelOnWindow(IModelPreview* preview, xlGraphicsContext* ctx,
                                      xlGraphicsProgram* solidProgram, xlGraphicsProgram* transparentProgram, bool is_3d = false,
                                      const xlColor* color = nullptr, bool allowSelected = false, bool wiring = false,
                                      bool highlightFirst = false, int highlightpixel = 0,
                                      float* boundingBox = nullptr) override;
    virtual void DisplayEffectOnWindow(IModelPreview* preview, double pointSize) override;

private:
    struct Vertex {
        float x, y, z;
        xlColor c;
    };
    // Everything one draw of the fountain needs, in model space: centered on the box, not rotated, world units.
    struct Scene {
        std::vector<Vertex> water;
        std::vector<Vertex> spray;
        std::vector<Vertex> glowTris;
        std::vector<Vertex> poolTris;
        std::vector<Vertex> solidTris;
        std::vector<Vertex> lines;
        float unitsPerMeter = 1.0f;
    };
    struct Burst {
        int pump;
        double t;
        double y;
        double speed;
        xlColor color;
    };

    void BuildScene(Scene& scene, IModelPreview* preview, float width, float height, bool is3d, bool layoutMode);
    // A channel's value 0-255 as its node holds it (a red, green or blue node keeps only its own component).
    [[nodiscard]] uint8_t NodeLevel(int node) const;
    [[nodiscard]] const SequenceData* RenderedSequence() const;
    [[nodiscard]] int PumpNodeChannel(int pump) const;
    [[nodiscard]] int LightNodeChannel(int pump, int color) const;
    void EnsureSeeds();
    void EmitScene(const Scene& scene, xlGraphicsProgram* solidProgram, xlGraphicsProgram* transparentProgram,
                   float waterPx, float sprayPx, const std::function<void(xlGraphicsContext*)>& push);

    int _pumpCount = 8;
    Arrangement _arrangement = Arrangement::Ring;
    bool _clockwise = false;
    int _pumpChannel = 1;
    bool _hasLights = true;
    int _lightChannel = 9;
    float _maxHeightFt = 15.0f;
    float _centerMaxHeightFt = 0.0f; // 0: the same as _maxHeightFt
    int _nozzleSpread = 80;
    bool _spray = true;
    bool _showPool = true;
    int _droplets = 700;
    bool _updateNodeNames = false;

    // droplet seeds (phase, angle, spread, speed jitter), fixed so the water doesn't shimmer between redraws
    std::vector<float> _seeds;
    std::vector<float> _spraySeeds;
    int _seededPumps = -1;
    int _seededDroplets = -1;

    // the spray: heads replayed from the sequence, explosions kept with the playhead time they happen at
    FountainPhysics::CollisionTracker _collisions;
    long long _headsK = LLONG_MIN;
    const void* _headsSource = nullptr;
    std::vector<Burst> _bursts;
};
