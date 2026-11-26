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

#include <algorithm>
#include <memory>
#include <vector>

// How real fountain pumps and their water behave, measured on a fountain (video at 60 fps, 30-100% from dead
// off and back off), shared with the Fountain Studio web visualizer so both draw the same water:
//
//  - Air drag: water flies as a projectile slowed by quadratic drag (k = 0.12 per meter). To reach a height it
//    has to leave faster than without air, gets there a little sooner, and comes down slower.
//  - The pump answers a change one sample (20 ms) late. Up to PUMP_JUMP m/s it is there at once; above that its
//    launch speed builds by PUMP_RISE m/s per second. Lowering or shutting off is immediate, and water already
//    in the air flies on.
//
// Heights are meters above the pool.
namespace FountainPhysics {

constexpr double G = 9.8;
// Pump command history samples per second.
constexpr int HISTORY_RATE = 50;
constexpr double DT = 1.0 / HISTORY_RATE;
// Water leaves the nozzle this high (m).
constexpr double NOZZLE = 0.08;
// Quadratic air drag, per meter: deceleration = k v^2.
constexpr double DRAG = 0.12;
// The pump's delay, in samples.
constexpr int PUMP_DELAY = 1;
// Launch speed (m/s) the pump gets to at once.
constexpr double PUMP_JUMP = 6.5;
// Above that, how fast the launch speed builds (m/s per second).
constexpr double PUMP_RISE = 20.0;
constexpr double FEET = 0.3048;

double TerminalSpeed();
// The launch speed that throws water `rise` meters above the nozzle.
double SpeedFor(double rise);
// Seconds water launched at v takes to its top.
double ApexTime(double v);
// How high water launched at v gets.
double ApexHeight(double v);
// Height of water launched at v, `age` seconds later (below 0 once it has landed).
double HeightAt(double v, double age);
// Seconds water launched at v is in the air, up and back down to the pool.
double AirTime(double v);

// One launch speed's flight, with its trigonometry done once (HeightAt for many droplets of the same launch).
struct Flight {
    explicit Flight(double v);
    // Height `age` seconds after launch (below 0 once it has landed).
    double Height(double age) const;

    double v;
    double apexTime;
    double apexHeight;

private:
    double _th;
    double _cosTh;
};
// Seconds water thrown `rise` meters up is in the air.
double FlightTime(double rise);
// Samples back the pump response looks (its longest ramp: up to vMax from PUMP_JUMP), past the delay.
int ResponseSamples(double vMax);

// The speed the pump actually launches at, from the commanded speeds before it: cmd(back) is the commanded
// speed `back` samples ago. It is the lowest of the delayed command and every earlier command's ramp (instant
// down, up at PUMP_RISE above PUMP_JUMP).
template<class Cmd>
double LaunchSpeed(Cmd&& cmd, int samples) {
    double v = cmd(PUMP_DELAY);
    for (int i = 1; i <= samples; ++i) {
        double ramp = std::max(cmd(PUMP_DELAY + i), PUMP_JUMP) + PUMP_RISE * DT * (i - 0.5);
        if (ramp < v) {
            v = ramp;
        }
    }
    return v;
}

// Where a steady stream with launch speed v tops out.
double TopOf(double v);

struct BurstEvent {
    int jet = 0;
    double t = 0;      // seconds (playhead)
    double y = 0;      // meters above the pool
    double speed = 0;  // closing speed, m/s: how hard the water collides
};

// The top of a group of water in the air: `attached` tops the stream the pump is feeding, the others fall free.
struct HeadView {
    int jet = 0;
    bool attached = false;
    double y = 0; // meters above the pool
};

// Explosions where falling water lands on rising water of the same pump.
//
// Every sample a parcel of water leaves the nozzle at the pump's launch speed and flies as a projectile with
// air drag. A head is the top of a group of parcels that form one stream:
//
//  1. Turning a pump on starts a head at the nozzle; it rises as fast as the water gets there.
//  2. When a new parcel can't keep up with the water above it, the pump dropped faster than gravity: that higher
//     water lets go as a free ball and the water below carries on as a new head. Shutting the pump off lets the
//     whole group go.
//  3. Two heads of one pump that meet moving toward each other (a falling one onto a rising one) explode there;
//     the strength is their closing speed. Heads that meet any other way merge quietly.
//
// Fed once per history sample with each pump's real launch speed (LaunchSpeed).
class CollisionTracker {
public:
    void Reset(int count);
    void Step(int jet, double t, double v);
    // Explosions since the last call.
    std::vector<BurstEvent> Take();
    // Heads created for one pump so far.
    int Made(int jet) const;
    // Heads in the air for one pump (attached plus free).
    int HeadCount(int jet) const;
    // Every head, as of the last sample.
    std::vector<HeadView> Heads() const;

private:
    struct Parcel {
        double t0;
        double v;
    };
    struct Ball {
        int id = 0;
        double y = 0;
        double v = 0;
        std::vector<Parcel> parcels;
        bool hasReleased = false;
        double released = 0;
    };
    struct JetState {
        std::shared_ptr<Ball> head;
        std::vector<std::shared_ptr<Ball>> free;
        double t = 0;
        double v = 0;
        int made = 0;
    };

    std::shared_ptr<Ball> NewHead(JetState& s, std::vector<Parcel> parcels, double t);
    static void LetGo(JetState& s, const std::shared_ptr<Ball>& b, double t);

    std::vector<JetState> _jets;
    std::vector<BurstEvent> _events;
    int _nextId = 1;
};

} // namespace FountainPhysics
