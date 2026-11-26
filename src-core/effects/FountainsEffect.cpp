/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "FountainsEffect.h"
#include "../graphics/xlGraphicsAccumulators.h"
#include "../render/Effect.h"
#include "../render/EffectLayer.h"
#include "../render/Element.h"
#include "../render/RenderBuffer.h"
#include "../render/RenderContext.h"
#include "../render/SequenceElements.h"
#include "../render/ValueCurve.h"
#include "../models/DMX/DmxFountain.h"
#include "../models/Model.h"
#include "UtilFunctions.h"

#include "../../include/Fountains.xpm"

FountainsEffect::FountainsEffect(int i) :
    RenderableEffect(i, "Fountains", Fountains, Fountains, Fountains, Fountains, Fountains) {
}

FountainsEffect::~FountainsEffect() {
}

namespace {
// The pattern settings. `prefix` is "" for render settings and "E_" for an effect's own (as the grid reads them).
struct Pattern {
    bool chase = false;
    std::string direction = "Forward";
    float cycles = 1.0f;
    int pumpOffset = 0;
    float overlap = 0.0f;

    Pattern(const SettingsMap& s, const std::string& prefix) {
        // "Sequential" (saved before Chase took it over) is a chase with no overlap
        const std::string type = s.Get(prefix + "CHOICE_Fountains_Type", "All");
        chase = type == "Chase" || type == "Sequential";
        direction = s.Get(prefix + "CHOICE_Fountains_Direction", "Forward");
        cycles = std::max(0.0f, s.GetFloat(prefix + "TEXTCTRL_Fountains_Cycles", 1.0f));
        pumpOffset = std::max(0, s.GetInt(prefix + "SLIDER_Fountains_PumpOffset", 0));
        overlap = type == "Sequential" ? 0.0f : std::clamp(s.GetInt(prefix + "SLIDER_Fountains_Overlap", 0), 0, 95) / 100.0f;
    }
};

// The order the units take their turn in.
std::vector<int> TurnOrder(int units, const std::string& direction) {
    std::vector<int> fwd;
    for (int i = 0; i < units; ++i) {
        fwd.push_back(i);
    }
    std::vector<int> rev(fwd.rbegin(), fwd.rend());
    if (direction == "Reverse") {
        return rev;
    }
    const bool bounce = direction == "Forward Bounce" || direction == "Reverse Bounce";
    std::vector<int> order = direction == "Reverse Bounce" ? rev : fwd;
    if (bounce && units > 2) {
        // there and back, without playing either end twice in a row
        std::vector<int> back = direction == "Reverse Bounce" ? fwd : rev;
        order.insert(order.end(), back.begin() + 1, back.end() - 1);
    }
    return order;
}

// Where each of `units` units is in the curve (0-1) at `position` through the effect, or -1 when it isn't its turn.
std::vector<float> UnitTimes(const Pattern& p, int units, float position) {
    // how far through the current pass (All); the very end of the effect is the end of the last pass, not a restart
    auto loop = [](float t) {
        const float u = t - std::floor(t);
        return (u == 0.0f && t > 0.0f) ? 1.0f : u;
    };
    std::vector<float> local(std::max(0, units), -1.0f);
    if (units <= 0) {
        return local;
    }
    if (!p.chase) {
        std::fill(local.begin(), local.end(), loop(position * p.cycles));
        return local;
    }
    const std::vector<int> order = TurnOrder(units, p.direction);
    const int turns = (int)order.size();
    // a pass is `turns` steps, one turn starting each step and lasting `len`: longer than a step by the overlap (none:
    // one pump at a time). The turns run on round the loop, so the first pump starts its next turn on time even while
    // the last ones are still finishing, and a turn still going when the effect ends is cut off there.
    const float step = 1.0f / turns;
    const float len = step / (1.0f - p.overlap);
    float t = position * p.cycles; // passes since the effect started
    if (position >= 1.0f && t > 0.0f) {
        t = std::nextafter(t, 0.0f); // the last frame ends the last pass rather than starting another
    }
    std::vector<float> latest(units, -1.0f);
    for (int turn = 0; turn < turns; ++turn) {
        // this turn's most recent start: none before the effect starts
        const float offset = turn * step;
        if (t < offset) {
            continue;
        }
        const float start = std::floor(t - offset) + offset;
        const int unit = (order[turn] + p.pumpOffset) % units;
        if (start < latest[unit]) {
            continue; // a more recent turn of the same unit (bouncing) has it
        }
        latest[unit] = start;
        const float l = (t - start) / len;
        local[unit] = l < 1.0f ? l : -1.0f;
    }
    return local;
}

// The units taking turns in a buffer's nodes: each pump (with its light) of a fountain, or each node of anything
// else, numbered in the order the nodes list them. unitOf / kindOf (1 pump, 2 light or other model, 0 untouched)
// per node. hasLights / hasPumps: whether any node takes a colour / a pump level.
int FindUnits(const std::vector<NodeBaseClassPtr>& nodes, std::vector<int>* unitOf, std::vector<int>* kindOf,
              bool* hasLights = nullptr, bool* hasPumps = nullptr) {
    std::map<std::pair<const Model*, int>, int> units;
    if (hasLights != nullptr) {
        *hasLights = false;
    }
    if (hasPumps != nullptr) {
        *hasPumps = false;
    }
    if (unitOf != nullptr) {
        unitOf->assign(nodes.size(), -1);
    }
    if (kindOf != nullptr) {
        kindOf->assign(nodes.size(), 0);
    }
    for (int n = 0; n < (int)nodes.size(); ++n) {
        const NodeBaseClass* node = nodes[n].get();
        if (node == nullptr) {
            continue;
        }
        const DmxFountain* f = dynamic_cast<const DmxFountain*>(node->model);
        const int kind = f != nullptr ? f->ChannelKind(*node) : 2; // not a fountain: colour it
        if (kind == 0) {
            continue; // a fountain channel that is neither pump nor light: leave it alone
        }
        const int key = f != nullptr ? f->ChannelPump(*node) : n;
        auto [it, added] = units.try_emplace(std::make_pair(node->model, key), (int)units.size());
        if (unitOf != nullptr) {
            (*unitOf)[n] = it->second;
        }
        if (kindOf != nullptr) {
            (*kindOf)[n] = kind;
        }
        if (hasLights != nullptr && kind == 2) {
            *hasLights = true;
        }
        if (hasPumps != nullptr && kind == 1) {
            *hasPumps = true;
        }
    }
    return (int)units.size();
}

// The curve, if the settings have one that is on.
bool LoadCurve(const SettingsMap& s, const std::string& prefix, ValueCurve& vc) {
    const std::string& curve = s.Get(prefix + "VALUECURVE_Fountains_Curves", xlEMPTY_STRING);
    if (curve.empty()) {
        return false;
    }
    vc.SetDivisor(1);
    vc.SetLimits(0, 100);
    vc.Deserialise(curve);
    return vc.IsActive();
}

// The units of the row an effect sits on (at least 1), and whether the row has lights and pumps (with no model: one
// unit, lights, no pumps).
int RowUnits(const Effect* e, bool* hasLights, bool* hasPumps) {
    if (hasLights != nullptr) {
        *hasLights = true;
    }
    if (hasPumps != nullptr) {
        *hasPumps = false;
    }
    const Element* el = e->GetParentEffectLayer() != nullptr ? e->GetParentEffectLayer()->GetParentElement() : nullptr;
    const SequenceElements* se = el != nullptr ? el->GetSequenceElements() : nullptr;
    RenderContext* rc = se != nullptr ? se->GetRenderContext() : nullptr;
    Model* m = rc != nullptr ? rc->GetModel(el->GetFullName()) : nullptr;
    if (m == nullptr) {
        return 1;
    }
    std::vector<NodeBaseClassPtr> nodes;
    int w = 0, h = 0;
    m->InitRenderBufferNodes("Default", "2D", "None", nodes, w, h, 0);
    return std::max(1, FindUnits(nodes, nullptr, nullptr, hasLights, hasPumps));
}
}

bool FountainsEffect::GetInlineEditCurveSpan(const Effect* e, float& start, float& scale) const {
    const Pattern p(e->GetSettings(), "E_");
    if (p.cycles <= 0.0f) {
        return false; // the curve never moves on
    }
    if (!p.chase) {
        start = 0.0f;
        scale = 1.0f / p.cycles;
        return true;
    }
    // the first pump's first turn (see UnitTimes)
    const int units = RowUnits(e, nullptr, nullptr);
    const std::vector<int> order = TurnOrder(units, p.direction);
    const float step = 1.0f / order.size();
    const float len = step / (1.0f - p.overlap);
    for (int turn = 0; turn < (int)order.size(); ++turn) {
        if ((order[turn] + p.pumpOffset) % units == 0) {
            start = turn * step / p.cycles;
            scale = len / p.cycles;
            return start < 1.0f;
        }
    }
    return false;
}

void FountainsEffect::Render(Effect* effect, const SettingsMap& settings, RenderBuffer& buffer) {
    const Pattern pattern(settings, "");
    ValueCurve vc;
    const bool hasCurve = LoadCurve(settings, "", vc);
    const float flatLevel = (float)settings.GetInt("SLIDER_Fountains_Curves", 0);

    const auto& nodes = buffer.GetNodes();
    std::vector<int> unitOf, kindOf;
    const int unitCount = FindUnits(nodes, &unitOf, &kindOf);
    if (unitCount == 0) {
        return;
    }
    const std::vector<float> local = UnitTimes(pattern, unitCount, buffer.GetEffectTimeIntervalPosition());

    std::vector<xlColor> pumpColour(unitCount, xlBLACK);
    std::vector<xlColor> lightColour(unitCount, xlBLACK);
    for (int u = 0; u < unitCount; ++u) {
        if (local[u] < 0.0f) {
            continue; // not its turn: pump off, light dark
        }
        float level = flatLevel;
        if (hasCurve) {
            level = vc.GetOutputValueAt(local[u], buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
            lightColour[u] = vc.GetColorAt(local[u]);
        }
        const uint8_t l = (uint8_t)std::lround(std::clamp(level, 0.0f, 100.0f) * 2.55f);
        pumpColour[u] = xlColor(l, l, l);
    }

    // pumps first, then lights: where a pump and a light share a cell (Per Preview) the light's colour is kept
    for (int pass = 1; pass <= 2; ++pass) {
        for (int n = 0; n < (int)nodes.size(); ++n) {
            if (kindOf[n] == pass) {
                buffer.SetNodePixel(n, pass == 1 ? pumpColour[unitOf[n]] : lightColour[unitOf[n]], true);
            }
        }
    }
}

// The effect in the grid, for the first pump over time (for a chase, the pattern decides when it plays and the rest of
// the time it is off). With colour keys and lights to show them on: its light's colour, with the pump's level as a
// line over it when the row has pumps too (when it hasn't, as high as the level with ramps on). Otherwise: the pump's
// level (0-100%) as a white area over black.
int FountainsEffect::DrawEffectBackground(const Effect* e, int x1, int y1, int x2, int y2,
                                          xlVertexColorAccumulator& bg, xlColor* colorMask, bool ramps) {
    const SettingsMap& settings = e->GetSettings();
    const Pattern pattern(settings, "E_");
    ValueCurve vc;
    const bool hasCurve = LoadCurve(settings, "E_", vc);
    const float flatLevel = (float)settings.GetInt("E_SLIDER_Fountains_Curves", 0);

    // the row's pumps (a chase's timing depends on how many) and whether it has lights and pumps
    bool hasLights = true;
    bool hasPumps = false;
    const int units = RowUnits(e, &hasLights, &hasPumps);
    const bool showColour = hasCurve && vc.HasColorKeys() && hasLights;

    const int width = x2 - x1;
    if (width <= 0) {
        return 2;
    }
    const int columns = std::clamp(width, 1, 400);
    const long startMS = e->GetStartTimeMS();
    const long endMS = e->GetEndTimeMS();
    // the first pump at `p` through the effect: where it is in the curve, or -1 when it is off
    auto timeAt = [&](float p) { return UnitTimes(pattern, units, p)[0]; };
    auto levelAt = [&](float t) {
        if (t < 0.0f) {
            return 0.0f;
        }
        return std::clamp(hasCurve ? vc.GetOutputValueAt(t, startMS, endMS) : flatLevel, 0.0f, 100.0f) / 100.0f;
    };
    auto quad = [&](float xa, float ya, float xb, float yb, float yBottom, const xlColor& c) {
        bg.AddVertex(xa, ya, c);
        bg.AddVertex(xa, yBottom, c);
        bg.AddVertex(xb, yBottom, c);
        bg.AddVertex(xb, yBottom, c);
        bg.AddVertex(xb, yb, c);
        bg.AddVertex(xa, ya, c);
    };

    if (showColour) {
        // the pump's level at each column edge, for the line
        std::vector<float> levels;
        if (hasPumps) {
            levels.reserve(columns + 1);
            for (int i = 0; i <= columns; ++i) {
                levels.push_back(levelAt(timeAt((float)i / columns)));
            }
            if (*std::max_element(levels.begin(), levels.end()) <= 0.0f) {
                levels.clear(); // the pump never runs: no line
            }
        }
        for (int i = 0; i < columns; ++i) {
            const float xa = x1 + (float)width * i / columns;
            const float xb = x1 + (float)width * (i + 1) / columns;
            const float t = timeAt((i + 0.5f) / columns);
            xlColor c = t >= 0.0f ? vc.GetColorAt(t) : xlBLACK;
            if (colorMask != nullptr) {
                c.ApplyMask(colorMask);
            }
            const float top = ramps && !hasPumps ? y2 - levelAt(t) * (y2 - y1) : (float)y1;
            quad(xa, top, xb, top, (float)y2, c);
        }
        if (!levels.empty()) {
            // a black line with a white edge, kept inside the row at 0 and 100%
            constexpr float outer = 2.0f;
            constexpr float inner = 1.0f;
            const float lo = y2 - outer;
            const float range = std::max(0.0f, (y2 - y1) - 2.0f * outer);
            auto px = [&](int i) { return x1 + (float)width * i / columns; };
            auto py = [&](int i) { return lo - levels[i] * range; };
            auto stroke = [&](float half, const xlColor& c) {
                for (int i = 0; i < columns; ++i) {
                    const float ax = px(i), ay = py(i), bx = px(i + 1), by = py(i + 1);
                    const float len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
                    if (len <= 0.0f) {
                        continue;
                    }
                    const float nx = -(by - ay) / len * half;
                    const float ny = (bx - ax) / len * half;
                    bg.AddVertex(ax + nx, ay + ny, c);
                    bg.AddVertex(ax - nx, ay - ny, c);
                    bg.AddVertex(bx - nx, by - ny, c);
                    bg.AddVertex(bx - nx, by - ny, c);
                    bg.AddVertex(bx + nx, by + ny, c);
                    bg.AddVertex(ax + nx, ay + ny, c);
                }
                // fill the corners between segments
                for (int i = 1; i < columns; ++i) {
                    quad(px(i) - half, py(i) - half, px(i) + half, py(i) - half, py(i) + half, c);
                }
            };
            stroke(outer, xlWHITE);
            stroke(inner, xlBLACK);
        }
        return 2; // a small icon over it
    }

    xlColor fill = xlWHITE;
    if (colorMask != nullptr) {
        fill.ApplyMask(colorMask);
    }
    quad((float)x1, (float)y1, (float)x2, (float)y1, (float)y2, xlBLACK);
    float prevX = (float)x1;
    float prevY = y2 - levelAt(timeAt(0.0f)) * (y2 - y1);
    for (int i = 1; i <= columns; ++i) {
        const float x = x1 + (float)width * i / columns;
        const float y = y2 - levelAt(timeAt((float)i / columns)) * (y2 - y1);
        if (prevY < y2 || y < y2) {
            quad(prevX, prevY, x, y, (float)y2, fill);
        }
        prevX = x;
        prevY = y;
    }
    return 2;
}
