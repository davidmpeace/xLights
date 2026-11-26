// DmxFountain and FountainPhysics: the water physics shared with the Fountain
// Studio web visualizer (its physics and heads tests, ported), the model's
// channels and serialization, and the previews flying the water from the
// rendered sequence.

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <pugixml.hpp>

#include "XmlSerializer/XmlSerializer.h"
#include "models/DMX/DmxFountain.h"
#include "models/DMX/FountainPhysics.h"
#include "models/DisplayAsType.h"
#include "models/SubModel.h"
#include "render/HeadlessRenderContext.h"
#include "render/RenderBuffer.h"
#include "render/SequenceData.h"

#include "test_dmx2_support.h"

using Catch::Approx;
using namespace dmx2;
namespace FP = FountainPhysics;

namespace {

// Launch speeds for a commanded speed profile over samples 0..n (commands before sample 0 are off).
std::vector<double> Launches(const std::function<double(int)>& cmd, int n, double vMax) {
    const int resp = FP::ResponseSamples(vMax);
    std::vector<double> out;
    for (int k = 0; k <= n; ++k) {
        out.push_back(FP::LaunchSpeed([&](int back) { return k - back < 0 ? 0.0 : cmd(k - back); }, resp));
    }
    return out;
}

const double V100 = FP::SpeedFor(5.18); // a 17 ft jet at 100%

struct Run {
    std::vector<FP::BurstEvent> events;
    FP::CollisionTracker tracker;
};

// Feeds one pump a launch-speed profile at 50 samples/s.
Run Feed(const std::function<double(double)>& profile, double seconds) {
    Run r;
    r.tracker.Reset(1);
    for (int k = 0; k <= (int)std::lround(seconds * 50); ++k) {
        r.tracker.Step(0, k / 50.0, profile(k / 50.0));
        for (auto& e : r.tracker.Take()) {
            r.events.push_back(e);
        }
    }
    return r;
}

double Solve(const std::function<double(double)>& f, double a, double b) {
    for (int n = 0; n < 60; ++n) {
        double m = (a + b) / 2;
        if ((f(m) > 0) == (f(a) > 0)) {
            a = m;
        } else {
            b = m;
        }
    }
    return (a + b) / 2;
}

std::string Serialize(const Model* m) {
    pugi::xml_document doc;
    XmlSerializingVisitor v(&doc);
    m->Accept(v);
    std::ostringstream os;
    doc.save(os, "", pugi::format_raw);
    return os.str();
}

// A show whose open sequence the fountain can read back, frame by frame.
struct SeqCtx : HeadlessRenderContext {
    SequenceData data;
    const SequenceData* GetRenderedSequenceData() const override { return &data; }
};

struct FountainShow {
    SeqCtx ctx;
    DmxFountain* f = nullptr;

    explicit FountainShow(const std::string& attrs = "") {
        pugi::xml_document doc;
        std::string xml = "<model name=\"Fountain\" DisplayAs=\"DmxFountain\" StartChannel=\"1\" StringType=\"Single Color White\" "
                          "WorldPosX=\"0\" WorldPosY=\"0\" WorldPosZ=\"0\" ScaleX=\"10\" ScaleY=\"20\" ScaleZ=\"10\" "
                          "FountainPumps=\"1\" FountainLights=\"1\" FountainPumpChannel=\"1\" FountainLightChannel=\"2\" "
                          "FountainMaxHeight=\"15\" DmxChannelCount=\"4\" " + attrs + "/>";
        REQUIRE(doc.load_string(xml.c_str()));
        f = dynamic_cast<DmxFountain*>(ctx.AllModels.createAndAddModel(doc.first_child(), 100, 100));
        REQUIRE(f != nullptr);
        ctx.AllModels.RecalcStartChannels();
        ctx.data.init(4, 400, 50); // 20 s at 20 fps
    }

    // Pump 1 at `level` (0-255) from `from` to `to` seconds.
    void Pump(double from, double to, uint8_t level, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) {
        for (unsigned int fr = 0; fr < ctx.data.NumFrames(); ++fr) {
            double t = fr * 0.05;
            if (t >= from - 1e-9 && t < to - 1e-9) {
                ctx.data[fr][0] = level;
                ctx.data[fr][1] = r;
                ctx.data[fr][2] = g;
                ctx.data[fr][3] = b;
            }
        }
    }

    // The water drawn by the sequencer's model preview at `seconds` (heights in model units, box -10..10).
    std::vector<RecVertex> Draw(double seconds) {
        FakePreview p;
        p.frameTime = (uint32_t)std::lround(seconds * 1000);
        p.width = 400;
        p.height = 400;
        f->DisplayEffectOnWindow(&p, 2.0);
        p.Run();
        return p.TransAcc().v;
    }
};

float MaxY(const std::vector<RecVertex>& v) {
    float m = -1e9f;
    for (auto& x : v) {
        m = std::max(m, x.y);
    }
    return m;
}

float MinY(const std::vector<RecVertex>& v) {
    float m = 1e9f;
    for (auto& x : v) {
        m = std::min(m, x.y);
    }
    return m;
}

} // namespace

// --- physics -------------------------------------------------------------------

TEST_CASE("Fountain water flies with air drag: the right height, up sooner and down slower than without air", "[fountain]") {
    const double v = FP::SpeedFor(3.5);
    CHECK(FP::ApexHeight(v) == Approx(FP::NOZZLE + 3.5).epsilon(1e-6));
    CHECK(FP::HeightAt(v, FP::ApexTime(v)) == Approx(FP::ApexHeight(v)).epsilon(1e-6));
    const double vacuum = std::sqrt(2 * 3.5 / 9.8);
    CHECK(FP::ApexTime(v) < vacuum);
    CHECK(FP::AirTime(v) - FP::ApexTime(v) > vacuum);
    CHECK(FP::HeightAt(v, FP::AirTime(v)) == Approx(0.0).margin(1e-4));
    // far past landing the descent stays finite (the overflow-free log(cosh))
    CHECK(FP::HeightAt(v, 500.0) < -1000.0);
}

TEST_CASE("Fountain pump answers one sample late, jumps, ramps above the jump and drops at once", "[fountain]") {
    const double V = FP::SpeedFor(5);
    auto on = Launches([&](int) { return V; }, 40, V);
    CHECK(on[0] == 0.0);
    CHECK(on[1] == Approx(FP::PUMP_JUMP + FP::PUMP_RISE * FP::DT * 0.5));
    CHECK(on[2] - on[1] == Approx(FP::PUMP_RISE * FP::DT));
    CHECK(on[40] == Approx(V));
    CHECK(Launches([](int) { return 5.0; }, 3, V)[1] == 5.0); // a low setting is there at once
    auto drop = Launches([&](int k) { return k < 30 ? V : 3.0; }, 32, V);
    CHECK(drop[30] == Approx(V));
    CHECK(drop[31] == 3.0);
    auto up = Launches([&](int k) { return k < 30 ? 3.0 : V; }, 33, V);
    CHECK(up[31] == Approx(FP::PUMP_JUMP + FP::PUMP_RISE * FP::DT * 0.5));
}

// --- heads and spray -----------------------------------------------------------

TEST_CASE("Fountain heads: turning a pump on makes one head that holds at the top", "[fountain]") {
    Run r = Feed([](double) { return V100; }, 5);
    CHECK(r.events.empty());
    CHECK(r.tracker.Made(0) == 1);
    auto heads = r.tracker.Heads();
    REQUIRE(heads.size() == 1);
    CHECK(heads[0].attached);
    CHECK(heads[0].y == Approx(FP::TopOf(V100)).margin(0.01));
}

TEST_CASE("Fountain heads: a stream dropping faster than gravity lets the old head go", "[fountain]") {
    Run r = Feed([](double t) { return t < 2 ? V100 : V100 * 0.6; }, 2.1);
    CHECK(r.tracker.Made(0) == 2);
    int attached = 0;
    double freeY = 0;
    for (auto& h : r.tracker.Heads()) {
        if (h.attached) {
            ++attached;
        } else {
            freeY = h.y;
        }
    }
    CHECK(attached == 1);
    CHECK(freeY > 4.5); // the old head, falling from the old top
    // and it lands on the lower stream quietly
    CHECK(Feed([](double t) { return t < 2 ? V100 : V100 * 0.6; }, 6).events.empty());
}

TEST_CASE("Fountain heads: shutting off lets the head go", "[fountain]") {
    Run r = Feed([](double t) { return t < 2 ? V100 : 0.0; }, 2.2);
    CHECK(r.tracker.Made(0) == 1);
    auto heads = r.tracker.Heads();
    REQUIRE(heads.size() == 1);
    CHECK_FALSE(heads[0].attached);
}

TEST_CASE("Fountain spray: water coming back on meets the old water falling, where the parabolas cross", "[fountain]") {
    Run r = Feed([](double t) { return t < 2 || t >= 2.6 ? V100 : 0.0; }, 6);
    CHECK(r.tracker.Made(0) == 2);
    REQUIRE(r.events.size() == 1);
    // the last old water (slot ending 1.99) and the first new water (slot starting 2.59) fly the same path 0.6 s apart
    const double t1 = 1.99, t2 = 2.59;
    const double t = Solve([&](double x) { return FP::HeightAt(V100, x - t1) - FP::HeightAt(V100, x - t2); },
                           t2 + FP::ApexTime(V100) * 0.01, t2 + FP::ApexTime(V100));
    CHECK(r.events[0].t == Approx(t).margin(0.05));
    CHECK(r.events[0].y == Approx(FP::HeightAt(V100, t - t2)).margin(0.5));
}

TEST_CASE("Fountain spray: a longer break hits lower and harder, and after the old water landed there is nothing to hit", "[fountain]") {
    auto at = [](double gap) { return Feed([gap](double t) { return t < 2 || t >= 2 + gap ? V100 : 0.0; }, 8).events; };
    auto a = at(0.25), b = at(1), c = at(1.5);
    REQUIRE(!a.empty());
    REQUIRE(!b.empty());
    REQUIRE(!c.empty());
    CHECK(a[0].y > b[0].y);
    CHECK(b[0].y > c[0].y);
    CHECK(a[0].speed < b[0].speed);
    CHECK(b[0].speed < c[0].speed);
    CHECK(at(2.5).empty());
}

TEST_CASE("Fountain heads: lowering slowly keeps the head, lowering fast lets it go as one mass", "[fountain]") {
    auto ramp = [](double rate) { return [rate](double t) { return t < 2 ? V100 : std::max(V100 * 0.6, V100 - rate * (t - 2)); }; };
    CHECK(Feed(ramp(1), 9).tracker.Made(0) == 1);
    CHECK(Feed(ramp(3), 9).tracker.Made(0) == 1);
    Run fast = Feed(ramp(V100 * 0.4), 9);
    CHECK(fast.tracker.Made(0) > 1);
    CHECK(fast.tracker.Made(0) < 5);
    CHECK(fast.events.empty());
}

// --- the model -----------------------------------------------------------------

TEST_CASE("Fountain is a DMX display type", "[fountain]") {
    CHECK(DisplayAsTypeFromString("DmxFountain") == DisplayAsType::DmxFountain);
    CHECK(DisplayAsTypeToString(DisplayAsType::DmxFountain) == "DmxFountain");
    CHECK(IsDmxDisplayType(DisplayAsType::DmxFountain));
    CHECK(Model::DetermineClass("DmxFountain", false, false, false, "") == "DMX Special Purpose");
}

TEST_CASE("Fountain default model: 8 pumps then their RGB lights", "[fountain]") {
    Show s;
    std::unique_ptr<Model> m(s.mm.CreateDefaultModel("DmxFountain"));
    auto* f = As<DmxFountain>(m.get());
    CHECK(f->GetPumpCount() == 8);
    CHECK(f->GetPumpChannel() == 1);
    CHECK(f->GetLightChannel() == 9);
    CHECK(f->GetDmxChannelCount() == 32);
    auto names = f->GenerateNodeNames();
    REQUIRE(names.size() == 32);
    CHECK(names[0] == "Pump 1");
    CHECK(names[7] == "Pump 8");
    CHECK(names[8] == "Pump 1 Red");
    CHECK(names[10] == "Pump 1 Blue");
    CHECK(names[31] == "Pump 8 Blue");
    CHECK(f->GetPWMOutputs().size() == 32);
    CHECK(f->CheckModelSettings().empty());
}

TEST_CASE("Fountain grows its channel count to fit its pumps and lights, and flags overlaps", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "DmxChannelCount=\"2\" FountainPumps=\"4\" FountainPumpChannel=\"1\" FountainLightChannel=\"3\"")));
    CHECK(f->GetDmxChannelCount() == 14);
    bool overlap = false;
    for (auto& msg : f->CheckModelSettings()) {
        overlap |= msg.find("overlap") != std::string::npos;
    }
    CHECK(overlap);
}

TEST_CASE("Fountain ring numbers its pumps from the front, counter-clockwise by default", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"8\" FountainNozzleSpread=\"80\"")));
    auto pts = f->GetPumpLayout();
    REQUIRE(pts.size() == 8);
    CHECK(pts[0].first == Approx(0.0f).margin(1e-6));
    CHECK(pts[0].second == Approx(0.4f));  // pump 1 at the front
    CHECK(pts[2].first == Approx(0.4f));   // a quarter of the way round: on the right
    CHECK(pts[2].second == Approx(0.0f).margin(1e-6));
    f->SetClockwise(true);
    CHECK(f->GetPumpLayout()[2].first == Approx(-0.4f));
    f->SetArrangement(DmxFountain::Arrangement::Line);
    pts = f->GetPumpLayout();
    CHECK(pts.front().first == Approx(-0.4f));
    CHECK(pts.back().first == Approx(0.4f));
    CHECK(pts[3].second == 0.0f);
}

TEST_CASE("Fountain ring with center puts the last pump in the middle", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"9\" FountainArrangement=\"RingCenter\" FountainNozzleSpread=\"80\"")));
    CHECK(f->GetArrangement() == DmxFountain::Arrangement::RingWithCenter);
    auto pts = f->GetPumpLayout();
    REQUIRE(pts.size() == 9);
    CHECK(pts[8].first == 0.0f);
    CHECK(pts[8].second == 0.0f);
    // the other eight go around the ring, pump 1 at the front, a quarter of the way round on the right
    CHECK(pts[0].second == Approx(0.4f));
    CHECK(pts[2].first == Approx(0.4f));
    for (int i = 0; i < 8; ++i) {
        CHECK(std::hypot(pts[i].first, pts[i].second) == Approx(0.4f));
    }
}

TEST_CASE("Fountain square spaces its pumps evenly around the edge", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"8\" FountainArrangement=\"Square\" FountainNozzleSpread=\"80\"")));
    auto pts = f->GetPumpLayout();
    REQUIRE(pts.size() == 8);
    for (auto& p : pts) {
        CHECK(std::max(std::fabs(p.first), std::fabs(p.second)) == Approx(0.4f)); // on the perimeter
    }
    // 8 around a square: the middle of each edge and the corners, starting at the front middle going right
    CHECK(pts[0].first == Approx(0.0f).margin(1e-6));
    CHECK(pts[0].second == Approx(0.4f));
    CHECK(pts[1].first == Approx(0.4f));
    CHECK(pts[1].second == Approx(0.4f));
    CHECK(pts[2].first == Approx(0.4f));
    CHECK(pts[2].second == Approx(0.0f).margin(1e-6));
    CHECK(pts[4].second == Approx(-0.4f));
    f->SetClockwise(true);
    CHECK(f->GetPumpLayout()[1].first == Approx(-0.4f));
}

TEST_CASE("Fountain grid fills rows from the front left, centring a short last row", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"7\" FountainArrangement=\"Grid\" FountainNozzleSpread=\"80\"")));
    auto pts = f->GetPumpLayout();
    REQUIRE(pts.size() == 7);
    // 3 columns, 3 rows: front row at z = +0.4, back row (one pump) centred at the back
    CHECK(pts[0].first == Approx(-0.4f));
    CHECK(pts[0].second == Approx(0.4f));
    CHECK(pts[2].first == Approx(0.4f));
    CHECK(pts[3].second == Approx(0.0f).margin(1e-6));
    CHECK(pts[6].first == Approx(0.0f).margin(1e-6));
    CHECK(pts[6].second == Approx(-0.4f));
}

TEST_CASE("Fountain arrangements map to the editors' list and back", "[fountain]") {
    REQUIRE(DmxFountain::ArrangementLabels().size() == (size_t)DmxFountain::Arrangement::Count);
    for (int i = 0; i < (int)DmxFountain::Arrangement::Count; ++i) {
        auto a = DmxFountain::ArrangementFromIndex(i);
        CHECK((int)a == i);
        CHECK(DmxFountain::ArrangementFromString(DmxFountain::ArrangementToString(a)) == a);
    }
    CHECK(DmxFountain::ArrangementFromIndex(99) == DmxFountain::Arrangement::Ring);
    CHECK(DmxFountain::ArrangementFromString("nonsense") == DmxFountain::Arrangement::Ring);
}

TEST_CASE("Fountain reads its pump levels and light colours from a frame", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"2\" FountainPumpChannel=\"1\" FountainLightChannel=\"3\" DmxChannelCount=\"8\"")));
    unsigned char frame[8] = { 255, 51, 10, 20, 30, 40, 50, 60 };
    CHECK(f->PumpLevel(frame, 8, 0) == Approx(1.0f));
    CHECK(f->PumpLevel(frame, 8, 1) == Approx(0.2f));
    CHECK(f->PumpLight(frame, 8, 0) == xlColor(10, 20, 30));
    CHECK(f->PumpLight(frame, 8, 1) == xlColor(40, 50, 60));
    CHECK(f->PumpLevel(frame, 1, 1) == 0.0f); // past the end of the frame
}

TEST_CASE("Fountain settings survive a save and load", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"6\" FountainArrangement=\"Grid\" FountainClockwise=\"1\" "
                                                         "FountainPumpChannel=\"3\" FountainLights=\"1\" FountainLightChannel=\"10\" "
                                                         "FountainMaxHeight=\"22.5\" FountainNozzleSpread=\"65\" FountainSpray=\"0\" "
                                                         "FountainPool=\"0\" FountainDroplets=\"400\" DmxChannelCount=\"27\"")));
    std::string xml = Serialize(f);
    CHECK(xml.find("DisplayAs=\"DmxFountain\"") != std::string::npos);

    Show s2;
    auto* g = As<DmxFountain>(s2.Add(xml));
    CHECK(g->GetPumpCount() == 6);
    CHECK(g->GetArrangement() == DmxFountain::Arrangement::Grid);
    CHECK(g->IsClockwise());
    CHECK(g->GetPumpChannel() == 3);
    CHECK(g->HasLights());
    CHECK(g->GetLightChannel() == 10);
    CHECK(g->GetMaxHeightFt() == Approx(22.5f));
    CHECK(g->GetNozzleSpread() == 65);
    CHECK_FALSE(g->IsSprayEnabled());
    CHECK_FALSE(g->IsPoolShown());
    CHECK(g->GetDropletsPerPump() == 400);
    CHECK(g->GetDmxChannelCount() == 27);
}

// --- lights as pixels and the automatic submodels ----------------------------------

namespace {
struct Cells {
    int w = 0, h = 0;
    std::vector<NodeBaseClassPtr> nodes;
};
Cells Buffer(const Model* m, const std::string& style) {
    Cells c;
    m->InitRenderBufferNodes(style, "2D", "None", c.nodes, c.w, c.h, 0);
    int w = 0, h = 0;
    m->GetBufferSize(style, "2D", "None", w, h, 0);
    CHECK(w == c.w);
    CHECK(h == c.h);
    return c;
}
// the 1-based channel of each node, by buffer cell
std::map<std::pair<int, int>, std::vector<int>> ByCell(const Cells& c, const Model* m) {
    std::map<std::pair<int, int>, std::vector<int>> cells;
    for (auto& n : c.nodes) {
        cells[{ n->Coords[0].bufX, n->Coords[0].bufY }].push_back((int)(n->ActChan - m->GetFirstChannel()) + 1);
    }
    return cells;
}
}

TEST_CASE("Fountain lights take their own colour from an effect", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"2\" FountainPumpChannel=\"1\" FountainLightChannel=\"3\" DmxChannelCount=\"8\"")));
    auto c = Buffer(f, "Default");
    auto& nodes = c.nodes;
    REQUIRE(nodes.size() == 8);
    xlColor orange(255, 128, 0);
    for (auto& n : nodes) {
        n->SetColor(orange);
    }
    unsigned char out[8];
    for (int i = 0; i < 8; ++i) {
        nodes[i]->GetForChannels(&out[i]);
    }
    CHECK(out[0] == 0);   // a pump takes the white in the colour
    CHECK(out[2] == 255); // light 1 red
    CHECK(out[3] == 128); // light 1 green
    CHECK(out[4] == 0);   // light 1 blue
    CHECK(out[5] == 255);
}

TEST_CASE("Fountain render buffer takes effect pixels like any model, not as a single-colour fixture", "[fountain]") {
    FountainShow fs;
    RenderBuffer rb(&fs.ctx, nullptr, fs.f);
    CHECK_FALSE(rb.IsDmxBuffer());
    rb.InitBuffer(1, 4, "None");
    rb.SetPixel(1, 0, xlRED); // what On, Color Wash and the rest do
    xlColor c;
    rb.GetPixel(1, 0, c);
    CHECK(c == xlRED);
    // a floodlight is still a fixture: its colour comes from pixel (0,0) through its colour ability
    pugi::xml_document doc;
    REQUIRE(doc.load_string("<model name=\"Flood\" DisplayAs=\"DmxFloodlight\" StartChannel=\"5\" StringType=\"Single Color White\" DmxChannelCount=\"3\"/>"));
    Model* flood = fs.ctx.AllModels.createAndAddModel(doc.first_child(), 100, 100);
    REQUIRE(flood != nullptr);
    RenderBuffer frb(&fs.ctx, nullptr, flood);
    CHECK(frb.IsDmxBuffer());
}

TEST_CASE("Fountain Default buffer stays one cell per channel, for the DMX effect", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"4\"")));
    auto c = Buffer(f, "Default");
    CHECK(c.w == 16);
    CHECK(c.h == 1);
    for (size_t i = 0; i < c.nodes.size(); ++i) {
        CHECK(c.nodes[i]->Coords[0].bufX == (int)i);
    }
    CHECK(f->GetBufferStyles() == std::vector<std::string>{ "Default", "Per Preview", "Single Line" });
}

TEST_CASE("Fountain Single Line gives each pump one cell, shared with its light", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"4\"")));
    auto c = Buffer(f, "Single Line");
    CHECK(c.w == 4);
    CHECK(c.h == 1);
    auto cells = ByCell(c, f);
    REQUIRE(cells.size() == 4);
    CHECK(cells[{ 0, 0 }] == std::vector<int>{ 1, 5, 6, 7 });  // pump 1, then its red, green, blue
    CHECK(cells[{ 3, 0 }] == std::vector<int>{ 4, 14, 15, 16 });
}

TEST_CASE("Fountain Per Preview puts a pump and its light at one spot", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "ScaleX=\"200\" ScaleY=\"100\" FountainPumps=\"4\" FountainArrangement=\"Line\"")));
    auto c = Buffer(f, "Per Preview");
    auto cells = ByCell(c, f);
    REQUIRE(cells.size() == 4); // seen from the front, four spots along the line
    for (auto& [cell, chans] : cells) {
        REQUIRE(chans.size() == 4);
        const int pump = chans[0];
        CHECK(chans == std::vector<int>{ pump, 5 + (pump - 1) * 3, 6 + (pump - 1) * 3, 7 + (pump - 1) * 3 });
    }
}

TEST_CASE("Fountain keeps Lights and Pumps submodels", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"8\"")));
    REQUIRE(f->GetNumSubModels() == 2);
    auto* pumps = dynamic_cast<SubModel*>(f->GetSubModel("Pumps"));
    auto* lights = dynamic_cast<SubModel*>(f->GetSubModel("Lights"));
    REQUIRE(pumps != nullptr);
    REQUIRE(lights != nullptr);
    CHECK(pumps->GetRange(0) == "1-8");
    CHECK(lights->GetRange(0) == "9-32");
    // a light is one cell, in its Default and Single Line buffers alike
    for (const char* style : { "Default", "Single Line" }) {
        INFO(style);
        auto c = Buffer(lights, style);
        CHECK(c.w == 8);
        CHECK(c.h == 1);
        auto cells = ByCell(c, f);
        CHECK(cells[{ 1, 0 }] == std::vector<int>{ 12, 13, 14 });
    }
    CHECK(Buffer(pumps, "Default").w == 8);
    // the submodels follow the channels
    f->SetPumpCount(4);
    f->AssignSequentialChannels();
    f->Reinitialize();
    REQUIRE(f->GetNumSubModels() == 2);
    CHECK(dynamic_cast<SubModel*>(f->GetSubModel("Lights"))->GetRange(0) == "5-16");
}

TEST_CASE("Fountain with a center pump adds Perimeter and Center submodels", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"9\" FountainArrangement=\"RingCenter\" FountainPumpChannel=\"1\" FountainLightChannel=\"10\" DmxChannelCount=\"36\"")));
    std::vector<std::string> names;
    for (auto* sm : f->GetSubModels()) {
        names.push_back(sm->GetName());
    }
    CHECK(names == std::vector<std::string>{ "Pumps", "Lights", "Perimeter", "Center", "Perimeter Pumps", "Center Pump", "Perimeter Lights", "Center Light" });
    auto range = [&](const char* n) { return dynamic_cast<SubModel*>(f->GetSubModel(n))->GetRange(0); };
    CHECK(range("Pumps") == "1-9");
    CHECK(range("Lights") == "10-36");
    CHECK(range("Perimeter Pumps") == "1-8");
    CHECK(range("Center Pump") == "9");
    CHECK(range("Perimeter Lights") == "10-33");
    CHECK(range("Center Light") == "34-36");
    CHECK(range("Center") == "9,34-36");
    CHECK(range("Perimeter").rfind("1,10-12,2,13-15,", 0) == 0);
    // Perimeter: a pump and its light per cell
    auto c = Buffer(f->GetSubModel("Perimeter"), "Default");
    CHECK(c.w == 8);
    CHECK(ByCell(c, f)[{ 0, 0 }] == std::vector<int>{ 1, 10, 11, 12 });

    // without the center pump the extra submodels go; the user's own stay
    auto* mine = new SubModel(f, "Mine", false, true, "Default");
    f->AddSubmodel(mine);
    mine->AddDefaultBuffer("1-2");
    f->SetArrangement(DmxFountain::Arrangement::Ring);
    f->Reinitialize();
    names.clear();
    for (auto* sm : f->GetSubModels()) {
        names.push_back(sm->GetName());
    }
    CHECK(names == std::vector<std::string>{ "Pumps", "Lights", "Mine" });
}

TEST_CASE("Fountain without lights keeps only pump submodels", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"5\" FountainLights=\"0\" FountainArrangement=\"RingCenter\" DmxChannelCount=\"5\"")));
    std::vector<std::string> names;
    for (auto* sm : f->GetSubModels()) {
        names.push_back(sm->GetName());
    }
    CHECK(names == std::vector<std::string>{ "Pumps", "Perimeter Pumps", "Center Pump" });
}

TEST_CASE("Fountain submodels survive a save and load without being rebuilt", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"3\"")));
    const std::string id = f->GetSubModel("Lights")->GetObjectId();
    std::string xml = Serialize(f);
    Show s2;
    auto* g = As<DmxFountain>(s2.Add(xml));
    CHECK(g->GetNumSubModels() == 2);
    CHECK(g->GetSubModel("Lights")->GetObjectId() == id);
}

TEST_CASE("Fountain center max height follows the max height until it is changed", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"9\" FountainArrangement=\"RingCenter\" FountainMaxHeight=\"15\"")));
    REQUIRE(f->HasCenterPump());
    CHECK_FALSE(f->IsCenterMaxHeightSet());
    CHECK(f->GetCenterMaxHeightFt() == Approx(15.0f));
    f->SetMaxHeightFt(18);
    CHECK(f->GetCenterMaxHeightFt() == Approx(18.0f)); // still following
    f->SetCenterMaxHeightFt(30);
    CHECK(f->IsCenterMaxHeightSet());
    f->SetMaxHeightFt(12);
    CHECK(f->GetCenterMaxHeightFt() == Approx(30.0f)); // its own now
    CHECK(f->GetPumpMaxHeightFt(0) == Approx(12.0f));
    CHECK(f->GetPumpMaxHeightFt(8) == Approx(30.0f)); // the center is the last pump
    f->SetCenterMaxHeightFt(12);
    CHECK_FALSE(f->IsCenterMaxHeightSet()); // back to following
    // only a Ring with Center has a center pump
    f->SetCenterMaxHeightFt(30);
    f->SetArrangement(DmxFountain::Arrangement::Ring);
    CHECK_FALSE(f->HasCenterPump());
    CHECK(f->GetPumpMaxHeightFt(8) == Approx(12.0f));
}

TEST_CASE("Fountain center max height survives a save and load", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "FountainPumps=\"5\" FountainArrangement=\"RingCenter\" FountainMaxHeight=\"10\" FountainCenterMaxHeight=\"25\"")));
    CHECK(f->GetCenterMaxHeightFt() == Approx(25.0f));
    Show s2;
    auto* g = As<DmxFountain>(s2.Add(Serialize(f)));
    CHECK(g->IsCenterMaxHeightSet());
    CHECK(g->GetCenterMaxHeightFt() == Approx(25.0f));
    // one that follows still follows after a load
    Show s3;
    auto* h = As<DmxFountain>(s3.Add(Dmx("DmxFountain", "FountainPumps=\"5\" FountainArrangement=\"RingCenter\" FountainMaxHeight=\"10\"")));
    Show s4;
    auto* k = As<DmxFountain>(s4.Add(Serialize(h)));
    CHECK_FALSE(k->IsCenterMaxHeightSet());
    k->SetMaxHeightFt(14);
    CHECK(k->GetCenterMaxHeightFt() == Approx(14.0f));
}

TEST_CASE("Fountain layout draws the center jet and the ring at their own heights", "[fountain]") {
    Show s;
    // ring at 15 ft, center at 30 ft: the box (y -10..10) is the center's height, the ring reaches about halfway
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "WorldPosX=\"0\" WorldPosY=\"0\" WorldPosZ=\"0\" ScaleX=\"10\" ScaleY=\"20\" "
                                                         "FountainPumps=\"5\" FountainArrangement=\"RingCenter\" FountainMaxHeight=\"15\" FountainCenterMaxHeight=\"30\"")));
    FakePreview p;
    xlColor sel = xlYELLOW;
    DrawLayout(f, p, &sel, true);
    float centerTop = -1e9f, ringTop = -1e9f;
    for (auto& v : p.TransAcc().v) {
        if (std::hypot(v.x, v.z) < 1.0f) {
            centerTop = std::max(centerTop, v.y);
        } else if (std::hypot(v.x, v.z) > 3.0f && std::hypot(v.x, v.z) < 5.0f) {
            ringTop = std::max(ringTop, v.y);
        }
    }
    const double U = 20.0 / (30 * FP::FEET + FP::NOZZLE);
    CHECK(centerTop > 9.0f);
    CHECK(centerTop <= 10.05f);
    CHECK(ringTop == Approx(-10.0 + (15 * FP::FEET + FP::NOZZLE) * U).margin(0.6));
}

// --- previews --------------------------------------------------------------------

TEST_CASE("Fountain layout shows every pump's water at full height", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "WorldPosX=\"0\" WorldPosY=\"0\" WorldPosZ=\"0\" ScaleX=\"10\" ScaleY=\"20\" ScaleZ=\"10\" FountainPumps=\"4\"")));
    FakePreview p;
    xlColor sel = xlYELLOW;
    DrawLayout(f, p, &sel, true);
    auto& v = p.TransAcc().v;
    REQUIRE(!v.empty());
    // the box is the pool (y = -10) up to the water at 100% (y = +10)
    CHECK(MaxY(v) > 9.0f);
    CHECK(MaxY(v) <= 10.05f);
    CHECK(MinY(v) >= -10.05f);
    // around the ring in 3D: water on both sides of the centre, front and back
    float minZ = 1e9f, maxZ = -1e9f;
    for (auto& x : v) {
        minZ = std::min(minZ, x.z);
        maxZ = std::max(maxZ, x.z);
    }
    CHECK(minZ < -3.0f);
    CHECK(maxZ > 3.0f);
}

TEST_CASE("Fountain preview without a sequence shows the levels the pumps have now", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "ScaleX=\"10\" ScaleY=\"20\" FountainPumps=\"1\" FountainLightChannel=\"2\" DmxChannelCount=\"4\"")));
    FakePreview off;
    DrawEffect(f, off);
    CHECK(off.TransAcc().v.empty());
    Chan(f, 1, 128); // half the max height
    FakePreview half;
    DrawEffect(f, half);
    auto& v = half.TransAcc().v;
    REQUIRE(!v.empty());
    CHECK(MaxY(v) < 0.5f);
    CHECK(MaxY(v) > -1.5f);
}

TEST_CASE("Fountain preview flies the water from the sequence: it climbs, and keeps flying after the pump stops", "[fountain]") {
    FountainShow fs;
    fs.Pump(0.0, 3.0, 255);
    // 100 ms after turning on the water hasn't got far
    auto rising = fs.Draw(0.1);
    REQUIRE(!rising.empty());
    CHECK(MaxY(rising) < -10.0f + 0.25f * 20.0f);
    // running steadily: up to the top
    auto steady = fs.Draw(2.5);
    CHECK(MaxY(steady) > 9.0f);
    CHECK(MinY(steady) < -9.0f);
    // 0.3 s after shutting off, the water already in the air still reaches the top, but nothing leaves the nozzle
    auto cut = fs.Draw(3.3);
    REQUIRE(!cut.empty());
    CHECK(MaxY(cut) > 9.0f);
    CHECK(MinY(cut) > -8.0f);
    // once it has all landed there is no water
    const double flight = FP::FlightTime(15 * FP::FEET);
    CHECK(fs.Draw(3.0 + flight + 0.3).empty());
}

TEST_CASE("Fountain preview is a function of the playhead: the same time draws the same water", "[fountain]") {
    FountainShow fs;
    fs.Pump(0.0, 2.0, 255);
    fs.Pump(2.6, 6.0, 255);
    auto a = fs.Draw(3.1);
    fs.Draw(5.0);
    auto b = fs.Draw(3.1); // a seek back replays to the same picture
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].y == Approx(b[i].y));
    }
}

TEST_CASE("Fountain preview sprays where returning water hits the old water, in the light's colour", "[fountain]") {
    FountainShow withSpray;
    FountainShow noSpray("FountainSpray=\"0\"");
    for (FountainShow* fs : { &withSpray, &noSpray }) {
        fs->Pump(0.0, 2.0, 255, 255, 0, 0);
        fs->Pump(2.6, 8.0, 255, 255, 0, 0);
    }
    bool sprayed = false;
    for (double t = 2.6; t < 5.0 && !sprayed; t += 0.05) {
        auto a = withSpray.Draw(t);
        auto b = noSpray.Draw(t);
        sprayed = a.size() > b.size();
    }
    CHECK(sprayed);
}

TEST_CASE("Fountain light ring shows its colour while the pump is off", "[fountain]") {
    Show s;
    auto* f = As<DmxFountain>(s.Add(Dmx("DmxFountain", "ScaleX=\"10\" ScaleY=\"20\" FountainPumps=\"1\" FountainLightChannel=\"2\" DmxChannelCount=\"4\"")));
    auto count = [](const RecColorAcc& a, const xlColor& c) {
        int n = 0;
        for (auto& v : a.v) {
            n += v.c.red == c.red && v.c.green == c.green && v.c.blue == c.blue;
        }
        return n;
    };
    // pump off, light off: a black ring, no water
    FakePreview dark;
    DrawEffect(f, dark);
    CHECK(count(dark.SolidAcc(), xlBLACK) == 24 * 6);
    // pump still off, light blue: the ring is blue and it glows
    Chan(f, 4, 255);
    FakePreview blue;
    DrawEffect(f, blue);
    CHECK(count(blue.SolidAcc(), xlBLUE) == 24 * 6);
    CHECK(!blue.TransAcc().v.empty());
    for (auto& v : blue.TransAcc().v) {
        CHECK(v.c.blue == 255); // only the glow: no water
    }
    // without lights there is no ring
    f->SetHasLights(false);
    FakePreview none;
    DrawEffect(f, none);
    CHECK(count(none.SolidAcc(), xlBLACK) == 0);
}

TEST_CASE("Fountain preview tints the water with the light under the pump", "[fountain]") {
    FountainShow fs;
    fs.Pump(0.0, 5.0, 255, 255, 0, 0);
    Chan(fs.f, 2, 255); // the light as the preview shows it now
    auto v = fs.Draw(2.0);
    REQUIRE(!v.empty());
    int red = 0;
    for (auto& x : v) {
        red += x.c.red > x.c.green * 2 && x.c.red > x.c.blue * 2;
    }
    CHECK(red > (int)v.size() / 2);
}
