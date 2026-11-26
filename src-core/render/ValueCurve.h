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

#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <list>
#include <vector>

#define MINVOID -91234
#define MAXVOID 91234
#define MINVOIDF -9.1234f
#define MAXVOIDF 9.1234f

#define VC_X_POINTS 200.0

#include "../utils/Color.h"

class AudioManager;
class SequenceElements;

// A colour keyframe on a value curve (for callers that let the curve carry a colour as well, like the Fountains effect).
struct vcColorKey {
    float x = 0.0f; // 0-1 across the curve
    xlColor color = xlBLACK;
    bool blend = true; // true: the colour before it fades into it; false: it cuts straight to it
};

class vcSortablePoint
{
public:

    static float Normalise(float v)
    {
        return std::round(v * VC_X_POINTS) / VC_X_POINTS;
    }
    static float perPoint()
    {
        return (float)(1.0f / VC_X_POINTS);
    }
    // How the curve leaves and arrives at a custom point. Hard: straight lines (a segment between two hard points is
    // linear). Ease: flat into and out of the point. Custom: Bezier handles the user places.
    enum class Shape : uint8_t {
        Hard = 0,
        Ease,
        Custom
    };

    float x;
    float y;
    bool wrapped; // y was wrapped into 0..1; a change between neighbours is a jump, not a slope
    Shape shape = Shape::Hard;
    // Custom handles, relative to the point (x as a fraction of the curve, y normalised 0-1): `in` toward the previous
    // point (dx <= 0), `out` toward the next (dx >= 0). Clamped to the segment when the curve is evaluated.
    float inDx = 0.0f;
    float inDy = 0.0f;
    float outDx = 0.0f;
    float outDy = 0.0f;
    vcSortablePoint(float xx, float yy, bool wrap)
    {
        x = Normalise(xx);
        y = yy;
        wrapped = wrap;
    }
    void CopyShape(const vcSortablePoint& o)
    {
        shape = o.shape;
        inDx = o.inDx;
        inDy = o.inDy;
        outDx = o.outDx;
        outDy = o.outDy;
    }
    bool IsHard() const { return shape == Shape::Hard; }
    bool IsNear(float xx, float yy) const
    {
        return (x == Normalise(xx) && yy >= y - 0.05 && yy <= y + 0.05);
    }
    void ClearWrap() { wrapped = false; }
    bool IsWrapped() const { return wrapped; }
    bool operator==(const vcSortablePoint& r) const
    {
        return x == r.x;
    }
    bool operator==(const float r) const
    {
        return x == Normalise(r);
    }
    bool operator<(const vcSortablePoint& r) const
    {
        return x < r.x;
    }
    bool operator<(const float r) const
    {
        return x < Normalise(r);
    }
    bool operator<=(const vcSortablePoint& r) const
    {
        return x <= r.x;
    }
    bool operator<=(const float r) const
    {
        return x <= Normalise(r);
    }
    bool operator>(const vcSortablePoint& r) const
    {
        return x > r.x;
    }
};

class ValueCurve
{
    std::list<vcSortablePoint> _values;
    std::list<vcSortablePoint> _baseCustomValues;
    std::vector<vcColorKey> _colorKeys; // sorted by x
    std::string _type;
    std::string _id;
    std::string _timingTrack;
    std::string _filterLabelText;
    bool _isFilterLabelRegex = false;
    float _max;
    float _min;
    float _viewMax = 0.0f; // the top of the editor's graph, in the same units as _max; 0 (or out of range): _max
    float _divisor;
    int _timeOffset;
    float _parameter1;
    float _parameter2;
    float _parameter3;
    float _parameter4;
    bool _active;
    bool _wrap;
    bool _realValues;
    bool _hasPreloadedValues = false; // true when Random values were loaded from serialized form, skip regeneration
    bool _hasStartEndLevel = false;   // true when P3/P4 Start/End Level was explicitly set by the user or loaded from serialized form
    std::string _audioTrackName; // "" = main; "Track1"/"Drums"/etc = alt
    static AudioManager* __audioManager;
    static std::map<std::string, AudioManager*> __altAudioManagers;
    static SequenceElements* __sequenceElements;

    std::string _cachedTimingTrack;
    long _cachedStartMS = -1;
    long _cachedEndMS = -1;
    std::vector<double> _cachedOffsets;
    void ClearCachedOffsets();

    void RenderType();
    void SetSerialisedValue(const std::string &k, const std::string &s);
    float SafeParameter(size_t p, float v);
    float Safe01(float v);
    void ConvertToRealValues(float oldmin, float oldmax);
    float Normalise(int parm, float value);
    float Denormalise(int parm, float value) const;
    float ApplyGain(float value, int gain) const;
    int GetSubsequentTimingMark(const std::string& timingTrack, int time, bool startsOnly, const std::string& filterLabelText, bool isFilterLabelRegex);
    int GetPriorTimingMark(const std::string& timingTrack, int time, bool startsOnly, const std::string& filterLabelText, bool isFilterLabelRegex);

public:

    static void SetAudio(AudioManager* am) { __audioManager = am; }
    static void SetAltAudio(const std::string& name, AudioManager* am) { __altAudioManagers[name] = am; }
    static void ClearAltAudio() { __altAudioManagers.clear(); }
    static AudioManager* GetAltAudio(const std::string& name) {
        auto it = __altAudioManagers.find(name);
        return (it != __altAudioManagers.end()) ? it->second : __audioManager;
    }
    static void SetSequenceElements(SequenceElements* se) { __sequenceElements = se; }
    static SequenceElements* GetSequenceElements() { return __sequenceElements; }
    static std::string GetValueCurveFolder(const std::string& showFolder);
    std::vector<double> GetTimingMarkOffsets(long startMS, long endMS) const;
    std::vector<double> GetTimingMarkOffsetsCached(long startMS, long endMS);
    std::vector<double> GetResampledOffsets(long startMS, long endMS, int cycles);

    void SetAudioTrack(const std::string& name) { _audioTrackName = name; }
    std::string GetAudioTrack() const { return _audioTrackName; }
    void SetStartEndLevelActive(bool v) { _hasStartEndLevel = v; }
    bool IsStartEndLevelActive() const { return _hasStartEndLevel; }

    ValueCurve() { _divisor = 1; _min = MINVOIDF; _max = MAXVOIDF; SetDefault(); }
    ValueCurve(const std::string& serialised);
    ValueCurve(const std::string& id, float min, float max = 100.0f, const std::string type = "Flat", float parameter1 = 0.0f, float parameter2 = 0.0f, float parameter3 = 0.0f, float parameter4 = 0.0f, bool wrap = false, float divisor = 1.0, const std::string& timingTrack = "", const std::string& filterLabelText = "", bool isFilterLabelRegex = false);
    void SetDefault(float min = MINVOIDF, float max = MAXVOIDF, int divisor = MAXVOID);
    std::string Serialise();
    void LoadXVC(const std::string& fn);
    void SaveXVC(const std::string& fn);
    static void GetRangeParm(int parm, const std::string& type, float& low, float& high);
    bool IsOk() const { return _id != ""; }
    void Deserialise(const std::string& s, bool holdminmax = false);
    void SetType(std::string type);
    void FixChangedScale(float oldmin, float oldmax, int divisor);
    void UnFixChangedScale(float newmin, float newmax);
    void ConvertChangedScale(float newmin, float newmax);
    float GetMax() const { assert(_max != MAXVOIDF); return _max; }
    float GetMin() const { assert(_min != MINVOIDF); return _min; }
    // The top of the value range the editor shows: a custom max the user picked below the real one, or the real max.
    // It only simplifies editing; the output range stays Min to Max.
    float GetViewMax() const { return HasViewMax() ? _viewMax : GetMax(); }
    bool HasViewMax() const { return _viewMax > _min && _viewMax < _max; }
    // Anything outside (Min, Max) clears it.
    void SetViewMax(float viewMax) { _viewMax = viewMax > _min && viewMax < _max ? viewMax : 0.0f; }
    int GetDivisor() const { assert(_divisor != MAXVOID); return (int)_divisor; }
    void SetRealValue() { _realValues = true; }
    void SetLimits(float min, float max) { _min = min; _max = max; }
    void FixScale(int scale);
    float GetValueAt(float offset, long startMS, long endMS);
    float GetOutputValueAt(float offset, long startMS, long endMS);
    float GetOutputValueAtDivided(float offset, long startMS, long endMS);
    float GetMaxValueDivided();
    float GetScaledValue(float offset) const;
    void SetActive(bool a) { _active = a; RenderType(); }
    bool IsActive() const { return _active && IsOk(); }
    void ToggleActive() { _active = !_active; if (_active) RenderType(); }
    void SetValueAt(float offset, float value, bool force = false);
    void DeletePoint(float offset);
    bool IsSetPoint(float offset);
    void SetDivisor(float divisor) { _divisor = divisor; }
    bool IsRealValue() const { return _realValues; }
    int GetPointCount() const { return _values.size(); }
    void SetTimingTrack(const std::string& timingTrack) { _timingTrack = timingTrack; ClearCachedOffsets(); }
    void SetFilterLabelText(const std::string& filterLabelText) {
    	_filterLabelText = filterLabelText;
		}
    void SetFilterLabelRegex(bool isFilterLabelRegex) {
        _isFilterLabelRegex = isFilterLabelRegex;
    }
    std::string GetFilterLabelText() const { return _filterLabelText; }
    bool IsFilterLabelRegex() const { return _isFilterLabelRegex; }
    std::string GetTimingTrack() const { return _timingTrack; }
    void SetParameter1(float parameter1) { _hasPreloadedValues = false; _parameter1 = SafeParameter(1, parameter1); RenderType(); }
    void SetParameter2(float parameter2) { _hasPreloadedValues = false; _parameter2 = SafeParameter(2, parameter2); RenderType(); }
    void SetParameter3(float parameter3) { _hasPreloadedValues = false; _parameter3 = SafeParameter(3, parameter3); RenderType(); }
    void SetParameter4(float parameter4) { _hasPreloadedValues = false; _parameter4 = SafeParameter(4, parameter4); RenderType(); }
    void SetTimeOffset(int timeOffset) { _timeOffset = timeOffset; RenderType(); }
    void SetWrap(bool wrap);
    int GetTimeOffset() const { return _timeOffset; }
    float GetParameter1() const { return _parameter1; }
    float GetParameter1_100() const;
    float GetParameter2() const { return _parameter2; }
    float GetParameter3() const { return _parameter3; }
    float GetParameter4() const { return _parameter4; }
    bool GetWrap() const { return _wrap; }
    std::string GetType() const { return _type; }
    std::list<vcSortablePoint> GetPoints() const { return _values; }
    void RemoveExcessCustomPoints();
    float FindMinPointLessThan(float point) const;
    float FindMaxPointGreaterThan(float point) const;
    bool NearCustomPoint(float x, float y) const;
    // The point at x (a set point), or nullptr.
    const vcSortablePoint* FindPoint(float x) const;
    // Sets the shape of the point at x; a new Custom shape starts as Ease (handles a third of the way to each neighbour, flat).
    void SetPointShape(float x, vcSortablePoint::Shape shape);
    // Copies the shape and handles of `from` onto the point at x.
    void CopyPointShape(float x, const vcSortablePoint& from);
    void SetPointHandles(float x, float inDx, float inDy, float outDx, float outDy);
    // y (normalised) on the segment from a to b at x, a.x <= x <= b.x: straight between hard points, a Bezier otherwise.
    static float SegmentValue(const vcSortablePoint& a, const vcSortablePoint& b, float x);
    // The Bezier control points of the segment from a to b, as GetValueAt uses them.
    static void SegmentControls(const vcSortablePoint& a, const vcSortablePoint& b, float& c1x, float& c1y, float& c2x, float& c2y);
    float GetPointAt(float x) const;
    void SetPointAt(float x, float y);
    std::string GetId() const { return _id; }
    void SetId(const std::string& id) { _id = id; }
    static void GetRangeParm1(const std::string& type, float& low, float &high);
    static void GetRangeParm2(const std::string& type, float& low, float &high);
    static void GetRangeParm3(const std::string& type, float& low, float &high);
    static void GetRangeParm4(const std::string& type, float& low, float &high);
    void Reverse();
    void Flip();
    void ConvertDivider(int oldDivider, int newDivider);
    void ScaleAndOffsetValues(float scale, int offset);
    void ReconstructBaseCustomValues();
    // Colour keyframes, independent of the curve type and its cycles. With none the colour is black throughout.
    [[nodiscard]] const std::vector<vcColorKey>& GetColorKeys() const { return _colorKeys; }
    [[nodiscard]] bool HasColorKeys() const { return !_colorKeys.empty(); }
    void SetColorKeys(std::vector<vcColorKey> keys);
    // The colour at offset (0-1). A single key is solid throughout. Otherwise: the first key's colour from the start,
    // each key fading or cutting from the one before, the last held to the end. With Wrap on, the start belongs to the
    // loop instead: a Blend first key fades in from the last key (so the curve loops seamlessly), a Hard one holds the
    // last colour until it cuts in.
    [[nodiscard]] xlColor GetColorAt(float offset) const;
    // A custom curve's one cycle of points (repeated Cycles times across the curve) replaced, e.g. after editing them.
    void SetBaseCustomPoints(const std::list<vcSortablePoint>& pts) { _baseCustomValues = pts; RenderType(); }
};
