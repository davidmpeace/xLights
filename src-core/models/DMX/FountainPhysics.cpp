/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "FountainPhysics.h"

#include <cmath>
#include <limits>

namespace FountainPhysics {

namespace {
constexpr double MIN_BURST = 1.0; // closing speed below this merges quietly
constexpr double LET_GO = 0.3;    // m: a new parcel peaking this far below the water above it can't keep up with it
constexpr double SAME_FALL = 0.5; // s: water separating this soon after a release joins that ball
constexpr double OFF = 0.05;      // launch speed below this is off
// -ffast-math folds infinity() away, so "nothing yet" is the lowest finite double.
constexpr double NONE = std::numeric_limits<double>::lowest();

// log(cosh x) without overflowing for large x
double LogCosh(double x) {
    x = std::fabs(x);
    return x + std::log(0.5 + 0.5 * std::exp(-2.0 * x));
}
}

double TerminalSpeed() {
    static const double vt = std::sqrt(G / DRAG);
    return vt;
}

double SpeedFor(double rise) {
    return TerminalSpeed() * std::sqrt(std::exp(2.0 * DRAG * std::max(0.0, rise)) - 1.0);
}

double ApexTime(double v) {
    const double vt = TerminalSpeed();
    return vt / G * std::atan(v / vt);
}

double ApexHeight(double v) {
    const double r = v / TerminalSpeed();
    return NOZZLE + std::log(1.0 + r * r) / (2.0 * DRAG);
}

Flight::Flight(double speed) :
    v(speed) {
    const double vt = TerminalSpeed();
    _th = std::atan(v / vt);
    _cosTh = std::cos(_th);
    apexTime = vt / G * _th;
    apexHeight = ApexHeight(v);
}

double Flight::Height(double age) const {
    const double vt = TerminalSpeed();
    if (age <= apexTime) {
        return NOZZLE + std::log(std::cos(_th - G * age / vt) / _cosTh) / DRAG;
    }
    return apexHeight - LogCosh(G * (age - apexTime) / vt) / DRAG;
}

double HeightAt(double v, double age) {
    return Flight(v).Height(age);
}

double AirTime(double v) {
    return ApexTime(v) + TerminalSpeed() / G * std::acosh(std::exp(DRAG * ApexHeight(v)));
}

double FlightTime(double rise) {
    return AirTime(SpeedFor(rise));
}

int ResponseSamples(double vMax) {
    return std::max(1, (int)std::ceil(std::max(0.0, vMax - PUMP_JUMP) / (PUMP_RISE * DT) + 0.5));
}

double TopOf(double v) {
    return v > OFF ? ApexHeight(v) : 0.0;
}

namespace {
// The highest of a parcel's water at time t. A parcel stands for everything launched in its sample slot, so
// the highest droplet is the one whose time in the air is closest to the time it takes to its top.
template<class P>
double HeightOf(const P& p, double t) {
    const double age = t - p.t0;
    if (age + DT / 2 < 0) {
        return NONE;
    }
    const double a = std::min(std::max(ApexTime(p.v), age - DT / 2), age + DT / 2);
    return HeightAt(p.v, a);
}

template<class P>
double TopAt(const std::vector<P>& parcels, double t) {
    double m = NONE;
    for (const auto& p : parcels) {
        m = std::max(m, HeightOf(p, t));
    }
    return m;
}

template<class P>
void MergeParcels(std::vector<P>& into, const std::vector<P>& from) {
    into.insert(into.end(), from.begin(), from.end());
    std::stable_sort(into.begin(), into.end(), [](const P& a, const P& b) { return a.t0 < b.t0; });
}
}

void CollisionTracker::Reset(int count) {
    _jets.assign(std::max(0, count), JetState());
    _events.clear();
}

std::shared_ptr<CollisionTracker::Ball> CollisionTracker::NewHead(JetState& s, std::vector<Parcel> parcels, double t) {
    s.made++;
    auto b = std::make_shared<Ball>();
    b->id = _nextId++;
    b->y = std::max(NOZZLE, TopAt(parcels, t));
    b->parcels = std::move(parcels);
    return b;
}

void CollisionTracker::LetGo(JetState& s, const std::shared_ptr<Ball>& b, double t) {
    b->hasReleased = true;
    b->released = t;
    s.free.push_back(b);
}

void CollisionTracker::Step(int jet, double t, double v) {
    if (jet < 0 || jet >= (int)_jets.size()) {
        return;
    }
    JetState& s = _jets[jet];
    s.t = t;
    s.v = v;
    if (v <= OFF) {
        if (s.head) {
            LetGo(s, s.head, t); // shut off: the water in the air flies on alone
            s.head = nullptr;
        }
    } else if (!s.head) {
        s.head = NewHead(s, { { t, v } }, t); // turned on
    } else {
        // when this parcel peaks, is some of the stream's water still well above it? That water lets go.
        const double peakAt = t + ApexTime(v);
        const double limit = TopOf(v) + LET_GO;
        auto& ps = s.head->parcels;
        int last = -1;
        for (int i = 0; i < (int)ps.size(); ++i) {
            if (HeightOf(ps[i], peakAt) > limit) {
                last = i;
            }
        }
        if (last >= 0) {
            std::vector<Parcel> peeled(ps.begin(), ps.begin() + last + 1);
            std::vector<Parcel> rest(ps.begin() + last + 1, ps.end());
            std::shared_ptr<Ball> same;
            for (auto& b : s.free) {
                if (b->hasReleased && t - b->released < SAME_FALL) {
                    same = b;
                    break;
                }
            }
            if (same) {
                MergeParcels(same->parcels, peeled); // the same fast drop, still going
                s.head->parcels = std::move(rest);
            } else {
                auto old = s.head;
                s.head = NewHead(s, std::move(rest), t);
                old->parcels = std::move(peeled);
                LetGo(s, old, t);
            }
        }
        s.head->parcels.push_back({ t, v });
    }

    // every head is the top of its water; parcels back in the pool are gone
    std::vector<std::shared_ptr<Ball>> balls;
    if (s.head) {
        balls.push_back(s.head);
    }
    balls.insert(balls.end(), s.free.begin(), s.free.end());
    std::vector<double> before;
    before.reserve(balls.size());
    for (auto& b : balls) {
        before.push_back(b->y);
    }
    for (auto& b : balls) {
        auto& p = b->parcels;
        p.erase(std::remove_if(p.begin(), p.end(), [t](const Parcel& x) { return HeightOf(x, t) <= 0; }), p.end());
        double y;
        if (b == s.head) {
            y = std::max(NOZZLE, TopAt(p, t));
        } else {
            y = p.empty() ? 0.0 : TopAt(p, t);
        }
        b->v = (y - b->y) / DT;
        b->y = y;
    }

    // heads that crossed this sample: the one that was above came down onto the one below
    std::vector<bool> gone(balls.size(), false);
    for (size_t u = 0; u < balls.size(); ++u) {
        for (size_t l = 0; l < balls.size(); ++l) {
            if (u == l || gone[u] || gone[l] || before[u] <= before[l] || balls[u]->y > balls[l]->y) {
                continue;
            }
            auto& up = balls[u];
            auto& low = balls[l];
            const double closing = low->v - up->v;
            if (up->v < 0 && low->v > 0 && closing >= MIN_BURST) {
                _events.push_back({ jet, t, (up->y + low->y) / 2.0, closing });
                gone[u] = true; // the falling water is spent in the explosion
            } else {
                const bool keepUp = up == s.head;
                auto& keep = keepUp ? up : low;
                auto& lose = keepUp ? low : up;
                MergeParcels(keep->parcels, lose->parcels); // merge quietly
                gone[keepUp ? l : u] = true;
            }
        }
    }
    std::vector<std::shared_ptr<Ball>> stillFree;
    for (auto& b : s.free) {
        bool isGone = false;
        for (size_t i = 0; i < balls.size(); ++i) {
            if (balls[i] == b && gone[i]) {
                isGone = true;
                break;
            }
        }
        if (!isGone && !b->parcels.empty()) {
            stillFree.push_back(b);
        }
    }
    s.free = std::move(stillFree);
}

std::vector<BurstEvent> CollisionTracker::Take() {
    std::vector<BurstEvent> out;
    out.swap(_events);
    return out;
}

int CollisionTracker::Made(int jet) const {
    return jet >= 0 && jet < (int)_jets.size() ? _jets[jet].made : 0;
}

int CollisionTracker::HeadCount(int jet) const {
    if (jet < 0 || jet >= (int)_jets.size()) {
        return 0;
    }
    return (_jets[jet].head ? 1 : 0) + (int)_jets[jet].free.size();
}

std::vector<HeadView> CollisionTracker::Heads() const {
    std::vector<HeadView> out;
    for (int j = 0; j < (int)_jets.size(); ++j) {
        if (_jets[j].head) {
            out.push_back({ j, true, _jets[j].head->y });
        }
        for (const auto& b : _jets[j].free) {
            out.push_back({ j, false, b->y });
        }
    }
    return out;
}

} // namespace FountainPhysics
