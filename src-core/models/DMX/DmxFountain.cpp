/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <spdlog/fmt/fmt.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <random>

#include "DmxFountain.h"
#include "DmxPresetAbility.h"
#include "../ModelManager.h"
#include "../PWMOutput.h"
#include "../SubModel.h"
#include "../../graphics/IModelPreview.h"
#include "../../graphics/xlGraphicsContext.h"
#include "../../graphics/xlGraphicsAccumulators.h"
#include "../../render/RenderContext.h"
#include "../../render/SequenceData.h"

namespace FP = FountainPhysics;

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr double PLUME = 0.5;                  // m/s a droplet drifts outward once the stream breaks up near the top
constexpr float DROPLET = 0.05f;               // m
constexpr float SPRAY_DROPLET = 0.13f;         // m
constexpr int SPRAY_SLOTS = 96;                // newest explosions drawn
constexpr int PER_BURST = 220;                 // droplets per explosion
constexpr double SPRAY_REACH = 1.25 * FP::FEET; // m: half the widest explosion (2.5 ft across)
constexpr double HARD = 10.0;                  // m/s closing speed that throws the widest
constexpr int REPLAY = 5;                      // s of the sequence replayed after a seek, longer than any water flies
constexpr double SPRAY_LIFE = 3.0;             // s an explosion is kept
constexpr float NOZZLE_RADIUS = 0.04f;         // m: a nozzle about 3" across
constexpr float GLOW_RADIUS = 0.1016f;         // m: its light an 8" disc
constexpr float POOL_MARGIN = 4.0f;            // ft of pool around a line of nozzles

float Clamp01(float v) {
    return std::min(1.0f, std::max(0.0f, v));
}

// Keeps a tint's hue, brightest channel at most 1.
xlColor Tint(float r, float g, float b, float alpha) {
    float m = std::max(r, std::max(g, b));
    if (m > 1.0f) {
        r /= m;
        g /= m;
        b /= m;
    }
    return xlColor((uint8_t)std::lround(Clamp01(r) * 255.0f), (uint8_t)std::lround(Clamp01(g) * 255.0f),
                   (uint8_t)std::lround(Clamp01(b) * 255.0f), (uint8_t)std::lround(Clamp01(alpha) * 255.0f));
}

constexpr float FLAT_2D = 0.35f; // a flat ring or disc on the ground, seen from the front in 2D

// A point at angle a, radius r around (cx, cy, cz): on the ground (x/z) in 3D, a flattened ellipse facing the
// viewer (x/y) in 2D.
template<class V>
V Around(float cx, float cy, float cz, float r, float a, const xlColor& c, bool is3d) {
    if (is3d) {
        return { cx + std::cos(a) * r, cy, cz + std::sin(a) * r, c };
    }
    return { cx + std::cos(a) * r, cy + std::sin(a) * r * FLAT_2D, cz, c };
}

template<class V>
void AddDisc(std::vector<V>& tris, float cx, float cy, float cz, float r, const xlColor& center, const xlColor& edge, bool is3d, int segments = 24) {
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * 2.0f * kPi;
        float a1 = (float)(i + 1) / segments * 2.0f * kPi;
        tris.push_back({ cx, cy, cz, center });
        tris.push_back(Around<V>(cx, cy, cz, r, a0, edge, is3d));
        tris.push_back(Around<V>(cx, cy, cz, r, a1, edge, is3d));
    }
}

// A flat ring (annulus) between radii r0 and r1.
template<class V>
void AddRing(std::vector<V>& tris, float cx, float cy, float cz, float r0, float r1, const xlColor& c, bool is3d, int segments = 24) {
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * 2.0f * kPi;
        float a1 = (float)(i + 1) / segments * 2.0f * kPi;
        V in0 = Around<V>(cx, cy, cz, r0, a0, c, is3d), in1 = Around<V>(cx, cy, cz, r0, a1, c, is3d);
        V out0 = Around<V>(cx, cy, cz, r1, a0, c, is3d), out1 = Around<V>(cx, cy, cz, r1, a1, c, is3d);
        tris.push_back(in0);
        tris.push_back(out0);
        tris.push_back(out1);
        tris.push_back(in0);
        tris.push_back(out1);
        tris.push_back(in1);
    }
}
}

DmxFountain::DmxFountain(const ModelManager& manager) :
    DmxModel(manager) {
    DisplayAs = DisplayAsType::DmxFountain;
}

DmxFountain::~DmxFountain() {
}

const std::vector<std::string>& DmxFountain::ArrangementLabels() {
    static const std::vector<std::string> labels = { "Ring", "Ring with Center", "Line", "Square", "Grid" };
    return labels;
}

DmxFountain::Arrangement DmxFountain::ArrangementFromIndex(int i) {
    return i >= 0 && i < (int)Arrangement::Count ? (Arrangement)i : Arrangement::Ring;
}

std::string DmxFountain::ArrangementToString(Arrangement a) {
    switch (a) {
    case Arrangement::RingWithCenter:
        return "RingCenter";
    case Arrangement::Line:
        return "Line";
    case Arrangement::Square:
        return "Square";
    case Arrangement::Grid:
        return "Grid";
    default:
        return "Ring";
    }
}

DmxFountain::Arrangement DmxFountain::ArrangementFromString(const std::string& s) {
    for (int i = 0; i < (int)Arrangement::Count; ++i) {
        if (ArrangementToString((Arrangement)i) == s) {
            return (Arrangement)i;
        }
    }
    return Arrangement::Ring;
}

int DmxFountain::GetRequiredChannelCount() const {
    int n = _pumpChannel + _pumpCount - 1;
    if (_hasLights) {
        n = std::max(n, _lightChannel + _pumpCount * 3 - 1);
    }
    return n;
}

void DmxFountain::AssignSequentialChannels() {
    _lightChannel = _pumpChannel + _pumpCount;
    _dmxChannelCount = GetRequiredChannelCount();
}

void DmxFountain::InitModel() {
    _dmxChannelCount = std::max(_dmxChannelCount, GetRequiredChannelCount());
    std::string names;
    for (const auto& n : GenerateNodeNames()) {
        names += n + ",";
    }
    SetNodeNames(names, _updateNodeNames);
    _updateNodeNames = false;

    DmxModel::InitModel();
    screenLocation.SetRenderSize(1, 1, 1);
    // the pool is round (or square): as deep as it is wide
    screenLocation.SetScaleZ(screenLocation.GetScaleX());

    // each light's channels take their own colour from an effect's pixels
    for (int i = 0; i < _pumpCount && _hasLights; ++i) {
        for (int c = 0; c < 3; ++c) {
            const int node = _lightChannel - 1 + i * 3 + c;
            if (node < 0 || node >= (int)Nodes.size()) {
                continue;
            }
            auto& old = Nodes[node];
            NodeBaseClass* n = c == 0 ? (NodeBaseClass*)new NodeClassRed(old->StringNum, 1, old->GetName())
                             : c == 1 ? (NodeBaseClass*)new NodeClassGreen(old->StringNum, 1, old->GetName())
                                      : (NodeBaseClass*)new NodeClassBlue(old->StringNum, 1, old->GetName());
            n->ActChan = old->ActChan;
            if (!old->Coords.empty()) {
                n->Coords[0] = old->Coords[0]; // a DMX channel has one
            }
            n->model = this;
            old.reset(n);
        }
    }

    // where each channel is, for Per Preview buffers: on the pool floor under its pump (anything else in the middle)
    for (auto& n : Nodes) {
        for (auto& c : n->Coords) {
            c.screenX = 0.0f;
            c.screenY = -0.5f;
            c.screenZ = 0.0f;
        }
    }
    const auto layout = GetPumpLayout();
    auto place = [&](int node, int pump) {
        if (node >= 0 && node < (int)Nodes.size()) {
            for (auto& c : Nodes[node]->Coords) {
                c.screenX = layout[pump].first;
                c.screenZ = layout[pump].second;
            }
        }
    };
    for (int i = 0; i < _pumpCount; ++i) {
        place(_pumpChannel - 1 + i, i);
        for (int c = 0; c < 3 && _hasLights; ++c) {
            place(_lightChannel - 1 + i * 3 + c, i);
        }
    }

    EnsureSeeds();
    _headsK = LLONG_MIN;
}

static const std::string DEFAULT_STYLE("Default");

const std::vector<std::string>& DmxFountain::GetBufferStyles() const {
    static const std::vector<std::string> styles{ DEFAULT_STYLE, "Per Preview", "Single Line" };
    return styles;
}

// Default is the DMX strip (one cell per channel, which the DMX effect writes by channel number); the others are
// the standard layouts, with each light's channels sharing a cell.
void DmxFountain::GetBufferSize(const std::string& type, const std::string& camera, const std::string& transform,
                                int& BufferWi, int& BufferHi, int stagger) const {
    std::string base = type.starts_with("Per Model ") ? type.substr(10) : type;
    if (base == DEFAULT_STYLE) {
        DmxModel::GetBufferSize(type, camera, transform, BufferWi, BufferHi, stagger);
    } else {
        Model::GetBufferSize(type, camera, transform, BufferWi, BufferHi, stagger);
    }
}

void DmxFountain::InitRenderBufferNodes(const std::string& type, const std::string& camera, const std::string& transform,
                                        std::vector<NodeBaseClassPtr>& newNodes, int& BufferWi, int& BufferHi, int stagger, bool deep) const {
    std::string base = type.starts_with("Per Model ") ? type.substr(10) : type;
    if (base == DEFAULT_STYLE) {
        DmxModel::InitRenderBufferNodes(type, camera, transform, newNodes, BufferWi, BufferHi, stagger, deep);
    } else {
        Model::InitRenderBufferNodes(type, camera, transform, newNodes, BufferWi, BufferHi, stagger, deep);
    }
}

int DmxFountain::GetNodeBufferGroup(const NodeBaseClass& node) const {
    if (Nodes.empty()) {
        return -1;
    }
    // a light's red, green and blue share a cell; a pump has its own (so one effect can set both, as Fountains does)
    const int ch = (int)node.ActChan - (int)GetFirstChannel() + 1;
    if (_hasLights && ch >= _lightChannel && ch < _lightChannel + _pumpCount * 3) {
        return (ch - _lightChannel) / 3;
    }
    return -1;
}

int DmxFountain::ChannelPump(const NodeBaseClass& node) const {
    if (Nodes.empty()) {
        return -1;
    }
    const int ch = (int)node.ActChan - (int)GetFirstChannel() + 1;
    if (ch >= _pumpChannel && ch < _pumpChannel + _pumpCount) {
        return ch - _pumpChannel;
    }
    if (_hasLights && ch >= _lightChannel && ch < _lightChannel + _pumpCount * 3) {
        return (ch - _lightChannel) / 3;
    }
    return -1;
}

int DmxFountain::ChannelKind(const NodeBaseClass& node) const {
    if (Nodes.empty()) {
        return 0;
    }
    const int ch = (int)node.ActChan - (int)GetFirstChannel() + 1;
    if (ch >= _pumpChannel && ch < _pumpChannel + _pumpCount) {
        return 1;
    }
    if (_hasLights && ch >= _lightChannel && ch < _lightChannel + _pumpCount * 3) {
        return 2;
    }
    return 0;
}

std::vector<std::pair<std::string, std::string>> DmxFountain::GetDefaultSubModels() const {
    auto span = [](int a, int b) { return a == b ? std::to_string(a) : fmt::format("{}-{}", a, b); };
    auto pumps = [&](int i0, int i1) { return span(_pumpChannel + i0, _pumpChannel + i1); };
    auto lights = [&](int i0, int i1) { return span(_lightChannel + i0 * 3, _lightChannel + i1 * 3 + 2); };
    // each pump followed by its light, so the two share a cell
    auto both = [&](int i0, int i1) {
        std::string r;
        for (int i = i0; i <= i1; ++i) {
            r += (r.empty() ? "" : ",") + pumps(i, i) + "," + lights(i, i);
        }
        return r;
    };
    const int n = _pumpCount;
    std::vector<std::pair<std::string, std::string>> subs;
    subs.emplace_back("Pumps", pumps(0, n - 1));
    if (_hasLights) {
        subs.emplace_back("Lights", lights(0, n - 1));
    }
    if (_arrangement == Arrangement::RingWithCenter && n >= 2) {
        const int c = n - 1; // the center is always the last pump
        if (_hasLights) {
            subs.emplace_back("Perimeter", both(0, c - 1));
            subs.emplace_back("Center", both(c, c));
        }
        subs.emplace_back("Perimeter Pumps", pumps(0, c - 1));
        subs.emplace_back("Center Pump", pumps(c, c));
        if (_hasLights) {
            subs.emplace_back("Perimeter Lights", lights(0, c - 1));
            subs.emplace_back("Center Light", lights(c, c));
        }
    }
    return subs;
}

// Only when the model is made: from then on they are the user's to change or delete.
void DmxFountain::CreateDefaultSubModels() {
    for (const auto& [name, ranges] : GetDefaultSubModels()) {
        if (GetSubModel(name) != nullptr) {
            continue;
        }
        SubModel* sm = new SubModel(this, name, false, true, DEFAULT_STYLE);
        AddSubmodel(sm);
        sm->AddDefaultBuffer(ranges);
        sm->CheckDuplicates();
    }
}

uint8_t DmxFountain::NodeLevel(int node) const {
    if (node < 0 || node >= (int)Nodes.size()) {
        return 0;
    }
    xlColor c;
    Nodes[node]->GetColor(c);
    return std::max(c.red, std::max(c.green, c.blue));
}

std::vector<std::string> DmxFountain::GenerateNodeNames() const {
    std::vector<std::string> names = DmxModel::GenerateNodeNames();
    auto name = [&](int ch, const std::string& n) {
        if (ch >= 1 && ch <= (int)names.size()) {
            names[ch - 1] = n;
        }
    };
    for (int i = 0; i < _pumpCount; ++i) {
        name(_pumpChannel + i, fmt::format("Pump {}", i + 1));
        if (_hasLights) {
            name(_lightChannel + i * 3, fmt::format("Pump {} Red", i + 1));
            name(_lightChannel + i * 3 + 1, fmt::format("Pump {} Green", i + 1));
            name(_lightChannel + i * 3 + 2, fmt::format("Pump {} Blue", i + 1));
        }
    }
    return names;
}

void DmxFountain::GetPWMOutputs(std::map<uint32_t, PWMOutput>& channels) const {
    DmxModel::GetPWMOutputs(channels);
    for (int i = 0; i < _pumpCount; ++i) {
        uint32_t ch = _pumpChannel + i;
        channels[ch] = PWMOutput(ch, PWMOutput::Type::LED, 1, fmt::format("Pump {}", i + 1));
        if (_hasLights) {
            static const char* rgb[] = { "Red", "Green", "Blue" };
            for (int c = 0; c < 3; ++c) {
                uint32_t lc = _lightChannel + i * 3 + c;
                channels[lc] = PWMOutput(lc, PWMOutput::Type::LED, 1, fmt::format("Pump {} {}", i + 1, rgb[c]));
            }
        }
    }
}

std::list<std::string> DmxFountain::CheckModelSettings() {
    std::list<std::string> res;
    if (GetRequiredChannelCount() > _dmxChannelCount) {
        res.push_back(fmt::format("    ERR: Fountain '{}' needs {} channels for its pumps and lights but has {}.", GetName(), GetRequiredChannelCount(), _dmxChannelCount));
    }
    if (_hasLights) {
        int pumpEnd = _pumpChannel + _pumpCount - 1;
        int lightEnd = _lightChannel + _pumpCount * 3 - 1;
        if (_pumpChannel <= lightEnd && _lightChannel <= pumpEnd) {
            res.push_back(fmt::format("    ERR: Fountain '{}' pump channels {}-{} overlap its light channels {}-{}.", GetName(), _pumpChannel, pumpEnd, _lightChannel, lightEnd));
        }
    }
    res.splice(res.end(), DmxModel::CheckModelSettings());
    return res;
}

std::vector<std::pair<float, float>> DmxFountain::GetPumpLayout() const {
    std::vector<std::pair<float, float>> pts;
    const float spread = _nozzleSpread / 100.0f;
    if (_pumpCount == 1) {
        pts.emplace_back(0.0f, 0.0f);
        return pts;
    }
    const float h = spread / 2.0f;
    const float dir = _clockwise ? -1.0f : 1.0f; // seen from above, counter-clockwise goes from the front to the right
    switch (_arrangement) {
    case Arrangement::Ring:
    case Arrangement::RingWithCenter: {
        const int around = _arrangement == Arrangement::Ring ? _pumpCount : _pumpCount - 1;
        for (int i = 0; i < around; ++i) {
            const float a = -dir * (float)i / around * 2.0f * kPi;
            pts.emplace_back(-std::sin(a) * h, std::cos(a) * h);
        }
        if (_arrangement == Arrangement::RingWithCenter) {
            pts.emplace_back(0.0f, 0.0f);
        }
        break;
    }
    case Arrangement::Line:
        for (int i = 0; i < _pumpCount; ++i) {
            pts.emplace_back(-h + spread * (float)i / (_pumpCount - 1), 0.0f);
        }
        break;
    case Arrangement::Square:
        // walk the perimeter from the middle of the front edge, an equal distance between pumps
        for (int i = 0; i < _pumpCount; ++i) {
            float d = 4.0f * spread * (float)i / _pumpCount; // distance along the edge
            float x, z;
            if (d < h) {
                x = d, z = h;
            } else if (d < 3 * h) {
                x = h, z = h - (d - h);
            } else if (d < 5 * h) {
                x = h - (d - 3 * h), z = -h;
            } else if (d < 7 * h) {
                x = -h, z = -h + (d - 5 * h);
            } else {
                x = -h + (d - 7 * h), z = h;
            }
            pts.emplace_back(dir * x, z);
        }
        break;
    case Arrangement::Grid: {
        // as square as it gets, front row first, left to right; a short last row is centered
        const int cols = (int)std::ceil(std::sqrt((double)_pumpCount));
        const int rows = (_pumpCount + cols - 1) / cols;
        const float dx = cols > 1 ? spread / (cols - 1) : 0.0f;
        const float dz = rows > 1 ? spread / (rows - 1) : 0.0f;
        for (int r = 0; r < rows; ++r) {
            const int inRow = std::min(cols, _pumpCount - r * cols);
            for (int c = 0; c < inRow; ++c) {
                pts.emplace_back((c - (inRow - 1) / 2.0f) * dx, rows > 1 ? h - r * dz : 0.0f);
            }
        }
        break;
    }
    default:
        break;
    }
    return pts;
}

int DmxFountain::PumpNodeChannel(int pump) const {
    int node = _pumpChannel - 1 + pump;
    if (node < 0 || node >= (int)Nodes.size()) {
        return -1;
    }
    return NodeStartChannel(node);
}

int DmxFountain::LightNodeChannel(int pump, int color) const {
    if (!_hasLights) {
        return -1;
    }
    int node = _lightChannel - 1 + pump * 3 + color;
    if (node < 0 || node >= (int)Nodes.size()) {
        return -1;
    }
    return NodeStartChannel(node);
}

float DmxFountain::PumpLevel(const unsigned char* frame, size_t numChannels, int pump) const {
    int ch = PumpNodeChannel(pump);
    if (frame == nullptr || ch < 0 || ch >= (int)numChannels) {
        return 0.0f;
    }
    return frame[ch] / 255.0f;
}

xlColor DmxFountain::PumpLight(const unsigned char* frame, size_t numChannels, int pump) const {
    uint8_t c[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; ++i) {
        int ch = LightNodeChannel(pump, i);
        if (frame != nullptr && ch >= 0 && ch < (int)numChannels) {
            c[i] = frame[ch];
        }
    }
    return xlColor(c[0], c[1], c[2]);
}

const SequenceData* DmxFountain::RenderedSequence() const {
    RenderContext* rc = GetModelManager().GetRenderContext();
    const SequenceData* sd = rc ? rc->GetRenderedSequenceData() : nullptr;
    if (sd == nullptr || sd->NumFrames() == 0 || sd->FrameTime() == 0) {
        return nullptr;
    }
    return sd;
}

void DmxFountain::EnsureSeeds() {
    if (_seededPumps == _pumpCount && _seededDroplets == _droplets) {
        return;
    }
    std::mt19937 rng(20240611u);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const float wide = _pumpCount == 1 ? 1.2f : 1.0f; // a single pump throws a slightly wider plume
    _seeds.resize((size_t)_pumpCount * _droplets * 4);
    for (size_t i = 0; i < _seeds.size(); i += 4) {
        _seeds[i] = u(rng);
        _seeds[i + 1] = u(rng) * 2.0f * kPi;
        _seeds[i + 2] = std::sqrt(u(rng)) * wide;
        _seeds[i + 3] = u(rng);
    }
    if (_spraySeeds.empty()) {
        _spraySeeds.resize((size_t)SPRAY_SLOTS * PER_BURST * 4);
        for (size_t i = 0; i < _spraySeeds.size(); i += 4) {
            _spraySeeds[i] = u(rng) * 2.0f * kPi;
            _spraySeeds[i + 1] = u(rng);
            _spraySeeds[i + 2] = u(rng);
            _spraySeeds[i + 3] = u(rng);
        }
    }
    _seededPumps = _pumpCount;
    _seededDroplets = _droplets;
}

// The fountain at the preview's playhead, a pure function of it like the Fountain Studio preview: the water in the
// air comes from what the sequence asked the pumps for over the last few seconds (sampled 50 times a second), so
// paused it stands still and after a seek it is what would be in the air there. Without a rendered sequence the
// pumps hold the levels they show now. In the layout every pump runs at 100% so the full height of the water shows.
void DmxFountain::BuildScene(Scene& scene, IModelPreview* preview, float width, float height, bool is3d, bool layoutMode) {
    EnsureSeeds();
    const int n = _pumpCount;
    // each pump's water at 100%; the box is as tall as the tallest, so the jets keep their heights relative to each other
    std::vector<double> maxH(n);
    for (int i = 0; i < n; ++i) {
        maxH[i] = GetPumpMaxHeightFt(i) * FP::FEET;
    }
    const double tallest = *std::max_element(maxH.begin(), maxH.end());
    const float U = (float)(height / (tallest + FP::NOZZLE)); // world units per meter
    const float baseY = -height / 2.0f;
    scene.unitsPerMeter = U;

    std::vector<float> jx(n), jz(n);
    {
        auto layout = GetPumpLayout();
        for (int i = 0; i < n; ++i) {
            jx[i] = layout[i].first * width;
            jz[i] = is3d ? layout[i].second * width : 0.0f;
        }
    }

    // what the pumps and lights show right now
    std::vector<float> levelNow(n, 0.0f);
    std::vector<xlColor> lightNow(n, xlBLACK);
    for (int i = 0; i < n; ++i) {
        levelNow[i] = NodeLevel(_pumpChannel - 1 + i) / 255.0f;
        if (_hasLights) {
            const int ln = _lightChannel - 1 + i * 3;
            lightNow[i] = xlColor(NodeLevel(ln), NodeLevel(ln + 1), NodeLevel(ln + 2));
        }
    }

    const SequenceData* sd = layoutMode ? nullptr : RenderedSequence();
    const double t = layoutMode ? 0.0 : preview->getCurrentFrameTime() / 1000.0;
    std::vector<int> pumpCh(n);
    for (int i = 0; i < n; ++i) {
        pumpCh[i] = PumpNodeChannel(i);
    }
    auto level = [&](int pump, long long k) -> double {
        if (layoutMode) {
            return 1.0;
        }
        if (sd == nullptr) {
            return levelNow[pump];
        }
        if (k < 0) {
            return 0.0;
        }
        long long frame = k * 1000 / FP::HISTORY_RATE / sd->FrameTime();
        int ch = pumpCh[pump];
        if (frame >= (long long)sd->NumFrames() || ch < 0 || ch >= (int)sd->NumChannels()) {
            return 0.0;
        }
        return *(*sd)[(unsigned int)frame][(unsigned int)ch] / 255.0;
    };

    // seconds a droplet cycles: the pump's longest flight, plus a little
    std::vector<double> period(n);
    for (int i = 0; i < n; ++i) {
        period[i] = FP::FlightTime(maxH[i]) + 0.25;
    }
    // the history covers the tallest pump's flight and spin-up
    const int resp = FP::ResponseSamples(FP::SpeedFor(tallest));
    const int len = std::min(1024, (int)std::ceil((FP::FlightTime(tallest) + 0.25) * FP::HISTORY_RATE) + FP::PUMP_DELAY + resp + 4);
    const long long now = (long long)std::floor(t * FP::HISTORY_RATE + 1e-6);
    const double lag = t - (double)now / FP::HISTORY_RATE;

    // the spray replays the heads from where it left off, or from REPLAY seconds back after a seek
    const bool sprayOn = _spray && sd != nullptr;
    long long sprayFrom = now + 1;
    if (!sprayOn) {
        _headsK = LLONG_MIN;
        _bursts.clear();
    } else if (now != _headsK) {
        if (_headsK == LLONG_MIN || now < _headsK || now - _headsK > FP::HISTORY_RATE || _headsSource != sd) {
            _collisions.Reset(n);
            _bursts.clear();
            sprayFrom = now - (long long)REPLAY * FP::HISTORY_RATE;
        } else {
            sprayFrom = _headsK + 1;
        }
    }

    // commanded launch speeds per pump, from the oldest sample anything needs up to now
    const long long cmdStart = std::min(now - (len - 1), sprayFrom) - FP::PUMP_DELAY - resp - 1;
    std::vector<std::vector<double>> cmd(n, std::vector<double>((size_t)(now - cmdStart + 1), 0.0));
    for (int i = 0; i < n; ++i) {
        for (long long k = cmdStart; k <= now; ++k) {
            cmd[i][(size_t)(k - cmdStart)] = FP::SpeedFor(std::max(0.0, level(i, k)) * maxH[i]);
        }
    }
    auto cmdAt = [&](int pump, long long k) {
        return k < cmdStart || k > now ? 0.0 : cmd[pump][(size_t)(k - cmdStart)];
    };
    // what the pump really launches at sample k (its delay and spin-up)
    auto launchAt = [&](int pump, long long k) {
        return FP::LaunchSpeed([&](int back) { return cmdAt(pump, k - back); }, resp);
    };

    if (sprayOn && sprayFrom <= now) {
        for (long long k = sprayFrom; k <= now; ++k) {
            for (int i = 0; i < n; ++i) {
                _collisions.Step(i, (double)k / FP::HISTORY_RATE, launchAt(i, k));
            }
            for (const auto& e : _collisions.Take()) {
                xlColor c = xlBLACK;
                if (_hasLights && k >= 0) {
                    long long frame = k * 1000 / FP::HISTORY_RATE / sd->FrameTime();
                    if (frame < (long long)sd->NumFrames()) {
                        const auto& fd = (*sd)[(unsigned int)frame];
                        c = PumpLight(fd[0], sd->NumChannels(), e.jet);
                    }
                }
                _bursts.push_back({ e.jet, e.t, e.y, e.speed, c });
            }
        }
        _bursts.erase(std::remove_if(_bursts.begin(), _bursts.end(), [t](const Burst& b) { return b.t <= t - SPRAY_LIFE; }), _bursts.end());
        _headsK = now;
        _headsSource = sd;
    }

    // the water
    scene.water.reserve((size_t)n * _droplets);
    std::vector<FP::Flight> flights;
    flights.reserve(len);
    for (int i = 0; i < n; ++i) {
        flights.clear();
        for (int back = 0; back < len; ++back) {
            flights.emplace_back(layoutMode || sd == nullptr ? cmdAt(i, now) : launchAt(i, now - back));
        }
        float lr = 0, lg = 0, lb = 0;
        if (!layoutMode) {
            lr = lightNow[i].red / 255.0f;
            lg = lightNow[i].green / 255.0f;
            lb = lightNow[i].blue / 255.0f;
        }
        const float bright = 0.45f + 0.55f * std::max(lr, std::max(lg, lb));
        // base water tint plus the light under it; the rising jet is brighter so it reads as a line
        xlColor tints[2][2];
        for (int falling = 0; falling < 2; ++falling) {
            for (int plume = 0; plume < 2; ++plume) {
                float k = bright * (falling ? 0.85f : 1.0f) * (plume ? 1.15f : 1.6f) * 2.5f;
                if (layoutMode) {
                    tints[falling][plume] = Tint(0.62f * k / 2.5f + 0.1f, 0.78f * k / 2.5f + 0.1f, 1.0f, plume ? 0.55f : 0.8f);
                } else {
                    tints[falling][plume] = Tint((0.30f + lr) * k, (0.34f + lg) * k, (0.40f + lb) * k, plume ? 0.55f : 0.8f);
                }
            }
        }
        const float* seeds = &_seeds[(size_t)i * _droplets * 4];
        for (int d = 0; d < _droplets; ++d, seeds += 4) {
            double phase = t / period[i] + seeds[0];
            double age = (phase - std::floor(phase)) * period[i];
            int back = (int)std::floor((age - lag) * FP::HISTORY_RATE + 0.5);
            back = std::clamp(back, 0, len - 1);
            const FP::Flight& f = flights[back];
            if (f.v < 0.05) {
                continue;
            }
            double y = f.Height(age);
            if (y < 0.05) {
                continue;
            }
            double after = std::max(0.0, age - f.apexTime * (0.82 + 0.12 * seeds[3]));
            double spread = seeds[2] * (0.012 + 0.008 * age + PLUME * after) * U;
            float x = jx[i] + std::cos(seeds[1]) * (float)spread;
            float z = jz[i] + (is3d ? std::sin(seeds[1]) * (float)spread : 0.0f);
            scene.water.push_back({ x, baseY + (float)y * U, z, tints[age > f.apexTime ? 1 : 0][after > 0 ? 1 : 0] });
        }
    }

    // spray: each explosion throws its droplets out, mostly sideways and up, slowing as the air stops them
    if (sprayOn && !_bursts.empty()) {
        size_t first = _bursts.size() > (size_t)SPRAY_SLOTS ? _bursts.size() - SPRAY_SLOTS : 0;
        for (size_t b = first; b < _bursts.size(); ++b) {
            const Burst& br = _bursts[b];
            if (br.pump < 0 || br.pump >= n) {
                continue;
            }
            double age = t - br.t;
            if (age < 0 || br.speed < 0.01) {
                continue;
            }
            const float* seeds = &_spraySeeds[(b - first) * PER_BURST * 4];
            for (int d = 0; d < PER_BURST; ++d, seeds += 4) {
                double life = 0.4 + 0.5 * seeds[3];
                if (age > life) {
                    continue;
                }
                double u = std::min(1.0, age / life);
                double reach = SPRAY_REACH * std::min(1.0, std::sqrt(br.speed / HARD)) * (0.3 + 0.7 * seeds[2]) * (1.0 - (1.0 - u) * (1.0 - u));
                double e = seeds[1] * 1.4 - 0.4;
                double c = std::sqrt(std::max(0.0, 1.0 - e * e));
                double py = br.y + e * reach;
                if (py < 0.03) {
                    continue;
                }
                double fade = (1.0 - u) * std::min(1.0, 0.4 + br.speed / 8.0);
                float r = 0.45f + br.color.red / 255.0f, g = 0.5f + br.color.green / 255.0f, bl = 0.56f + br.color.blue / 255.0f;
                scene.spray.push_back({ jx[br.pump] + std::cos(seeds[0]) * (float)(c * reach * U),
                                        baseY + (float)py * U,
                                        jz[br.pump] + (is3d ? std::sin(seeds[0]) * (float)(c * reach * U) : 0.0f),
                                        Tint(r * 1.6f, g * 1.6f, bl * 1.6f, (float)fade * 0.9f) });
            }
        }
    }

    // the pool, the nozzles and the lights under them
    const xlColor rim(58, 70, 82);
    if (_showPool && is3d) {
        const xlColor water(7, 19, 28, 170);
        const bool round = _arrangement == Arrangement::Ring || _arrangement == Arrangement::RingWithCenter || n == 1;
        if (round) {
            AddDisc(scene.poolTris, 0.0f, baseY, 0.0f, width / 2.0f, water, water, true, 64);
            for (int s = 0; s < 64; ++s) {
                float a0 = s / 64.0f * 2.0f * kPi, a1 = (s + 1) / 64.0f * 2.0f * kPi;
                scene.lines.push_back({ std::cos(a0) * width / 2.0f, baseY, std::sin(a0) * width / 2.0f, rim });
                scene.lines.push_back({ std::cos(a1) * width / 2.0f, baseY, std::sin(a1) * width / 2.0f, rim });
            }
        } else {
            // a line sits in a narrow pool, a square or grid in a square one
            float hw = width / 2.0f;
            float hd = _arrangement == Arrangement::Line ? std::min(hw, POOL_MARGIN * (float)FP::FEET * U) : hw;
            scene.poolTris.push_back({ -hw, baseY, -hd, water });
            scene.poolTris.push_back({ hw, baseY, -hd, water });
            scene.poolTris.push_back({ hw, baseY, hd, water });
            scene.poolTris.push_back({ -hw, baseY, -hd, water });
            scene.poolTris.push_back({ hw, baseY, hd, water });
            scene.poolTris.push_back({ -hw, baseY, hd, water });
            float corners[5][2] = { { -hw, -hd }, { hw, -hd }, { hw, hd }, { -hw, hd }, { -hw, -hd } };
            for (int s = 0; s < 4; ++s) {
                scene.lines.push_back({ corners[s][0], baseY, corners[s][1], rim });
                scene.lines.push_back({ corners[s + 1][0], baseY, corners[s + 1][1], rim });
            }
        }
    } else if (_showPool) {
        scene.lines.push_back({ -width / 2.0f, baseY, 0.0f, rim });
        scene.lines.push_back({ width / 2.0f, baseY, 0.0f, rim });
    }
    // the light ring around each nozzle shows its colour whether the pump runs or not (black when the light is off)
    const xlColor steel(138, 150, 163);
    const float ringOut = std::max(GLOW_RADIUS * U, width * 0.02f);
    const float ringIn = ringOut * 0.6f;
    const float nozzleR = std::min(std::max(NOZZLE_RADIUS * U, width * 0.006f), ringIn * 0.8f);
    const float lift = is3d ? 0.002f * U : 0.0f;
    for (int i = 0; i < n; ++i) {
        AddDisc(scene.solidTris, jx[i], baseY + lift, jz[i], nozzleR, steel, steel, is3d, 12);
        if (!_hasLights) {
            continue;
        }
        const xlColor light = layoutMode ? xlBLACK : lightNow[i];
        const bool lit = light.red || light.green || light.blue;
        AddRing(scene.solidTris, jx[i], baseY + lift, jz[i], ringIn, ringOut, light, is3d, 24);
        if (lit) {
            xlColor center(light.red, light.green, light.blue, 150);
            xlColor edge = center;
            edge.alpha = 0;
            AddDisc(scene.glowTris, jx[i], baseY + lift * 0.5f, jz[i], ringOut * 2.0f, center, edge, is3d, 24);
        }
    }
}

void DmxFountain::EmitScene(const Scene& scene, xlGraphicsProgram* solidProgram, xlGraphicsProgram* transparentProgram,
                            float waterPx, float sprayPx, const std::function<void(xlGraphicsContext*)>& push) {
    auto* tvac = transparentProgram->getAccumulator();
    tvac->PreAlloc(scene.poolTris.size() + scene.glowTris.size() + scene.water.size() + scene.spray.size());
    auto add = [](xlVertexColorAccumulator* vac, const std::vector<Vertex>& vs) {
        int start = vac->getCount();
        for (const auto& v : vs) {
            vac->AddVertex(v.x, v.y, v.z, v.c);
        }
        return std::make_pair(start, (int)vs.size());
    };
    auto pool = add(tvac, scene.poolTris);
    auto glow = add(tvac, scene.glowTris);
    auto water = add(tvac, scene.water);
    auto spray = add(tvac, scene.spray);
    transparentProgram->addStep([=](xlGraphicsContext* ctx) {
        push(ctx);
        if (pool.second) {
            ctx->drawTriangles(tvac, pool.first, pool.second);
        }
        if (glow.second) {
            ctx->drawTriangles(tvac, glow.first, glow.second);
        }
        if (water.second) {
            ctx->drawPoints(tvac, waterPx, true, water.first, water.second);
        }
        if (spray.second) {
            ctx->drawPoints(tvac, sprayPx, true, spray.first, spray.second);
        }
        ctx->PopMatrix();
    });

    auto* svac = solidProgram->getAccumulator();
    svac->PreAlloc(scene.solidTris.size() + scene.lines.size());
    auto solid = add(svac, scene.solidTris);
    auto lines = add(svac, scene.lines);
    solidProgram->addStep([=](xlGraphicsContext* ctx) {
        push(ctx);
        if (solid.second) {
            ctx->drawTriangles(svac, solid.first, solid.second);
        }
        if (lines.second) {
            ctx->drawLines(svac, lines.first, lines.second);
        }
        ctx->PopMatrix();
    });
}

void DmxFountain::DisplayModelOnWindow(IModelPreview* preview, xlGraphicsContext* ctx,
                                       xlGraphicsProgram* solidProgram, xlGraphicsProgram* transparentProgram, bool is_3d,
                                       const xlColor* c, bool allowSelected, bool wiring,
                                       bool highlightFirst, int highlightpixel,
                                       float* boundingBox) {
    if (!IsActive()) {
        return;
    }

    screenLocation.PrepareToDraw(is_3d, allowSelected);
    screenLocation.SetScaleZ(screenLocation.GetScaleX());
    const float width = std::max(0.001f, screenLocation.GetMWidth());
    const float height = std::max(0.001f, screenLocation.GetMHeight());
    // the selection box is as deep as the pool is wide
    const float depth = is_3d && screenLocation.GetScaleZ() > 0 ? width / screenLocation.GetScaleZ() : 1.0f;
    screenLocation.UpdateBoundingBox(1, 1, depth);
    if (boundingBox) {
        boundingBox[0] = -0.5;
        boundingBox[1] = -0.5;
        boundingBox[2] = -0.5f * depth;
        boundingBox[3] = 0.5;
        boundingBox[4] = 0.5;
        boundingBox[5] = 0.5f * depth;
    }

    Scene scene;
    BuildScene(scene, preview, width, height, is_3d, allowSelected);

    const glm::vec3 pos(screenLocation.GetHcenterPos(), screenLocation.GetVcenterPos(), is_3d ? screenLocation.GetDcenterPos() : 0.0f);
    const glm::vec3 rot = screenLocation.GetRotationAngles();
    const glm::mat4 placement = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(screenLocation.GetRotationQuat());

    // droplets are drawn as points, sized like the Fountain Studio preview: in 3D the droplet's size times half the
    // view's height over its distance from the camera (no focal-length factor, so they read as a fine spray), in
    // 2D its size on screen
    const float backing = (float)preview->getBackingScaleFactor();
    const float minPx = 2.5f * backing;
    const float maxPx = 16.0f * backing;
    auto pixels = [&](float worldSize) {
        float px;
        if (is_3d) {
            glm::vec4 eye = preview->GetViewMatrix() * placement * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            const float dist = std::fabs(eye.z);
            if (dist <= 1e-3f) {
                return minPx;
            }
            px = worldSize * preview->getHeight() / 2.0f / dist;
        } else {
            const glm::mat4& pv = preview->GetProjViewMatrix();
            glm::vec4 a = pv * placement * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::vec4 b = pv * placement * glm::vec4(0.0f, worldSize, 0.0f, 1.0f);
            if (a.w <= 1e-6f || b.w <= 1e-6f) {
                return minPx;
            }
            px = std::fabs(b.y / b.w - a.y / a.w) * preview->getHeight() / 2.0f;
        }
        return std::clamp(px * backing, minPx, maxPx);
    };
    const float waterPx = pixels(DROPLET * scene.unitsPerMeter);
    const float sprayPx = pixels(SPRAY_DROPLET * scene.unitsPerMeter);

    EmitScene(scene, solidProgram, transparentProgram, waterPx, sprayPx, [=](xlGraphicsContext* ctx) {
        ctx->PushMatrix();
        if (!is_3d) {
            // not 3d, flatten to the 0 plane
            ctx->Scale(1.0f, 1.0f, 0.0f);
        }
        ctx->Translate(pos.x, pos.y, pos.z);
        ctx->Rotate(rot.z, 0, 0, 1)->Rotate(rot.y, 0, 1, 0)->Rotate(rot.x, 1, 0, 0);
    });

    if ((Selected() || (Highlighted() && is_3d)) && c != nullptr && allowSelected) {
        if (is_3d) {
            GetModelScreenLocation().DrawHandles(transparentProgram, preview->GetCameraZoomForHandles(), preview->GetHandleScale(), Highlighted(), IsFromBase());
        } else {
            GetModelScreenLocation().DrawHandles(transparentProgram, preview->GetCameraZoomForHandles(), preview->GetHandleScale(), IsFromBase());
        }
    }
}

void DmxFountain::DisplayEffectOnWindow(IModelPreview* preview, double pointSize) {
    if (!IsActive() && preview->IsNoCurrentModel()) {
        return;
    }

    bool mustEnd = false;
    xlGraphicsContext* ctx = preview->getCurrentGraphicsContext();
    if (ctx == nullptr) {
        if (preview->StartDrawing(pointSize)) {
            ctx = preview->getCurrentGraphicsContext();
            mustEnd = true;
        }
    }
    if (ctx) {
        // a front view of the fountain, fitted to the window
        const float width = std::max(0.001f, screenLocation.GetMWidth());
        const float height = std::max(0.001f, screenLocation.GetMHeight());
        Scene scene;
        BuildScene(scene, preview, width, height, false, false);

        const int w = preview->getWidth();
        const int h = preview->getHeight();
        const float scale = std::min(w * 0.9f / width, h * 0.9f / height);
        const float backing = (float)preview->getBackingScaleFactor();
        const float minPx = 2.5f * backing;
        const float waterPx = std::clamp(DROPLET * scene.unitsPerMeter * scale * backing, minPx, 16.0f * backing);
        const float sprayPx = std::clamp(SPRAY_DROPLET * scene.unitsPerMeter * scale * backing, minPx, 16.0f * backing);

        EmitScene(scene, preview->getCurrentSolidProgram(), preview->getCurrentTransparentProgram(), waterPx, sprayPx,
                  [=](xlGraphicsContext* ctx) {
                      ctx->PushMatrix();
                      ctx->Translate(w / 2.0f, h / 2.0f, 0.5f);
                      ctx->Scale(scale, scale, 0.001f);
                  });
    }
    if (mustEnd) {
        preview->EndDrawing();
    }
}
