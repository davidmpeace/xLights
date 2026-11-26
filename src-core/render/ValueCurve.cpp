/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <pugixml.hpp>
#include <mutex>
#include "ValueCurve.h"
#include "xLightsVersion.h"
#include "RenderContext.h"
#include "UtilFunctions.h"
#include "utils/AppCallbacks.h"
#include "media/AudioManager.h"
#include "utils/ExternalHooks.h"
#include "SequenceElements.h"

#include <log.h>

#include <cassert>
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

AudioManager* ValueCurve::__audioManager = nullptr;
std::map<std::string, AudioManager*> ValueCurve::__altAudioManagers;
SequenceElements* ValueCurve::__sequenceElements = nullptr;

static bool IsStartEndLevelType(const std::string& type)
{
    return type == "Exponential Up" || type == "Exponential Down" ||
           type == "Logarithmic Up" || type == "Logarithmic Down" ||
           type == "Parabolic Down" || type == "Parabolic Up";
}

static std::string fmtVC(float v) {
    // Note: deliberately using snprintf rather than std::to_chars. The floating-point
    // overloads of std::to_chars are only available in Apple's libc++ starting with
    // macOS 13.3; xLights supports macOS 12, so using them causes a dyld "Symbol not
    // found" crash at launch on older systems.
    //
    // Three decimals so custom points keep their 1/VC_X_POINTS (0.005) x grid and finer
    // y, but a trailing zero is dropped so any value that fits in two decimals is
    // written exactly as before ("1.00", "0.50") and existing sequences don't churn.
    char buf[32];
    int n = std::snprintf(buf, sizeof(buf), "%.3f", v);
    if (n < 0) return {};
    if (static_cast<size_t>(n) >= sizeof(buf)) n = sizeof(buf) - 1;
    if (n > 0 && buf[n - 1] == '0') --n;
    return std::string(buf, n);
}

float ValueCurve::SafeParameter(size_t p, float v)
{
    float low;
    float high;
    ValueCurve::GetRangeParm(p, _type, low, high);

    if (low == MINVOID) low = _min;
    if (high == MAXVOID) high = _max;

    assert(_min != MINVOIDF);
    assert(_max != MAXVOIDF);

    return std::min(high, std::max(low, v));
}

float ValueCurve::Safe01(float v)
{
    return std::min(1.0f, std::max(0.0f, v));
}

void ValueCurve::GetRangeParm(int parm, const std::string& type, float& low, float &high)
{
    switch (parm)
    {
    case 1:
        ValueCurve::GetRangeParm1(type, low, high);
        break;
    case 2:
        ValueCurve::GetRangeParm2(type, low, high);
        break;
    case 3:
        ValueCurve::GetRangeParm3(type, low, high);
        break;
    case 4:
        ValueCurve::GetRangeParm4(type, low, high);
        break;
    default:
        break;
    }
}

float ValueCurve::Denormalise(int parm, float value) const
{
    float low;
    float high;
    ValueCurve::GetRangeParm(parm, _type, low, high);

    if (low == MINVOID)
    {
        assert(_min != MINVOIDF);
        low = _min;
    }
    if (high == MAXVOID)
    {
        assert(_max != MAXVOIDF);
        high = _max;
    }

    float res = value;

    if (low != 0 || high != 100)
    {
        float range = high - low;
        res = value * range / 100.0 + low;
    }

    if (res < low) res = low;
    if (res > high) res = high;

    return res;
}

std::string ValueCurve::GetValueCurveFolder(const std::string& showFolder)
{
    if (showFolder == "") return "";

    std::string vcf = showFolder + "/valuecurves";
    std::error_code ec;
    if (!std::filesystem::exists(vcf, ec))
    {
        std::filesystem::create_directory(vcf, ec);
        if (!std::filesystem::exists(vcf, ec))
        {
            return "";
        }
    }
    return vcf;
}

void ValueCurve::ConvertToRealValues(float oldmin, float oldmax)
{
    float min = _min;
    _min = oldmin;
    float max = _max;
    _max = oldmax;

    _parameter1 = Denormalise(1, _parameter1);
    _parameter2 = Denormalise(2, _parameter2);
    _parameter3 = Denormalise(3, _parameter3);
    _parameter4 = Denormalise(4, _parameter4);

    _min = min;
    _max = max;
}

void ValueCurve::GetRangeParm1(const std::string& type, float& low, float &high)
{
    low = 0;
    high = 100;

    if (type == "Custom")
    {
        low = 1;
        high = 10;
    }
    else if (type == "Flat")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Random")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Ramp")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Ramp Up/Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Ramp Up/Down Hold")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Saw Tooth")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Timing Track Toggle") {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Timing Track Fade Fixed") {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Timing Track Fade Proportional") {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Music")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Inverted Music")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Music Trigger Fade")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Square")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Parabolic Down")
    {
    }
    else if (type == "Parabolic Up")
    {
    }
    else if (type == "Logarithmic Up")
    {
    }
    else if (type == "Logarithmic Down")
    {
    }
    else if (type == "Exponential Up")
    {
    }
    else if (type == "Exponential Down")
    {
    }
    else if (type == "Sine")
    {
    }
    else if (type == "Decaying Sine")
    {
    }
    else if (type == "Abs Sine")
    {
    }
}

void ValueCurve::GetRangeParm2(const std::string& type, float& low, float &high)
{
    low = 0;
    high = 100;

    if (type == "Custom")
    {
    }
    else if (type == "Flat")
    {
    }
    else if (type == "Random")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Ramp")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Ramp Up/Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Timing Track Toggle") {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Timing Track Fade Fixed") {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Timing Track Fade Proportional") {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Inverted Music")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Music")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Music Trigger Fade")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Ramp Up/Down Hold")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Saw Tooth")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Square")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Parabolic Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Parabolic Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Logarithmic Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Logarithmic Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Exponential Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Exponential Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Sine")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Decaying Sine")
    {
        //low = MINVOID;
        //high = MAXVOID;
    }
    else if (type == "Abs Sine")
    {
        low = MINVOID;
        high = MAXVOID;
    }
}

void ValueCurve::GetRangeParm3(const std::string& type, float& low, float &high)
{
    low = 0;
    high = 100;

    if (type == "Custom")
    {
    }
    else if (type == "Flat")
    {
    }
    else if (type == "Ramp")
    {
    }
    else if (type == "Ramp Up/Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Random")
    {
        low = 1;
        high = VC_X_POINTS;
    }
    else if (type == "Ramp Up/Down Hold")
    {
    }
    else if (type == "Saw Tooth")
    {
    }
    else if (type == "Square")
    {
    }
    else if (type == "Parabolic Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Parabolic Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Logarithmic Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Logarithmic Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Exponential Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Exponential Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Sine")
    {
    }
    else if (type == "Decaying Sine")
    {
    }
    else if (type == "Abs Sine")
    {
    }
    else if (type == "Music Trigger Fade")
    {
    }
    else if (type == "Music")
    {
        low = -100;
        high = 100;
    }
    else if (type == "Inverted Music")
    {
        low = -100;
        high = 100;
    }
    else if (type == "Timing Track Toggle") {
    }
    else if (type == "Timing Track Fade Fixed") {
        low = 1;
        high = 1000;
    }
    else if (type == "Timing Track Fade Proportional") {
        low = 1;
        high = 100;
    }
}

void ValueCurve::GetRangeParm4(const std::string& type, float& low, float &high)
{
    low = 0;
    high = 100;

    if (type == "Custom")
    {
    }
    else if (type == "Flat")
    {
    }
    else if (type == "Ramp")
    {
    }
    else if (type == "Random")
    {
    }
    else if (type == "Ramp Up/Down")
    {
    }
    else if (type == "Ramp Up/Down Hold")
    {
    }
    else if (type == "Saw Tooth")
    {
    }
    else if (type == "Square")
    {
    }
    else if (type == "Parabolic Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Parabolic Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Logarithmic Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Logarithmic Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Exponential Up")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Exponential Down")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Sine")
    {
        low = MINVOID;
        high = MAXVOID;
    }
    else if (type == "Decaying Sine")
    {
    }
    else if (type == "Music Trigger Fade")
    {
    }
    else if (type == "Timing Track Toggle") {
    }
    else if (type == "Timing Track Fade Fixed") {
    }
    else if (type == "Timing Track Fade Proportional") {
    }
    else if (type == "Abs Sine")
    {
        low = MINVOID;
        high = MAXVOID;
    }
}

void ValueCurve::Reverse()
{
    // Only reverse the time offset if a non zero value was used
    if (_timeOffset != 0)
    {
        _timeOffset = 100 - _timeOffset;
    }

    if (!_colorKeys.empty()) {
        // the fade into key i+1 becomes the fade into key i once time runs backwards
        std::vector<vcColorKey> keys = _colorKeys;
        const bool firstBlend = keys.front().blend;
        for (size_t i = 0; i < keys.size(); ++i) {
            keys[i].blend = i + 1 < _colorKeys.size() ? _colorKeys[i + 1].blend : firstBlend;
            keys[i].x = 1.0f - keys[i].x;
        }
        SetColorKeys(std::move(keys));
    }

    if (_type == "Custom")
    {
        for (auto& it : _values)
        {
            it.x = 1.0 - it.x;
            // what led in now leads out
            float inDx = it.inDx, inDy = it.inDy;
            it.inDx = -it.outDx;
            it.inDy = it.outDy;
            it.outDx = -inDx;
            it.outDy = inDy;
        }
        _values.sort();
        if (_parameter1 == 1.0f) {
            _baseCustomValues = _values;
        }
    }
    else if (_type == "Ramp")
    {
        float p1 = GetParameter1();
        SetParameter1(GetParameter2());
        SetParameter2(p1);
    }
    else if (_type == "Ramp Up/Down")
    {
        float p1 = GetParameter1();
        SetParameter1(GetParameter3());
        SetParameter3(p1);
    }
    //else if (_type == "Logarithmic Up")
    //{
    //    float p1 = GetParameter1();
    //    float p2 = GetParameter2();
    //    SetType("Logarithmic Down");
    //    SetParameter1(100.0 - p1);
    //    SetParameter2(p2);
    //}
    //else if (_type == "Logarithmic Down")
    //{
    //    float p1 = GetParameter1();
    //    float p2 = GetParameter2();
    //    SetType("Logarithmic Up");
    //    SetParameter1(100.0 - p1);
    //    SetParameter2(p2);
    //}
    //else if (_type == "Exponential Up")
    //{
    //    float p1 = GetParameter1();
    //    float p2 = GetParameter2();
    //    SetType("Exponential Down");
    //    SetParameter1(100.0 - p1);
    //    SetParameter2(p2);
    //}
    //else if (_type == "Exponential Down")
    //{
    //    float p1 = GetParameter1();
    //    float p2 = GetParameter2();
    //    SetType("Exponential Up");
    //    SetParameter1(100.0 - p1);
    //    SetParameter2(p2);
    //}
    else if (_type == "Sine" || _type == "Abs Sine")
    {
        // y(x) = sin(x * 2pi * cycles + phase) reversed in time is another sine with phase pi - 2pi * cycles - phase
        // (|sin| has period pi, so the half cycle shift is not needed for Abs Sine)
        float p = (_type == "Sine" ? 50.0f : 0.0f) - 10.0f * GetParameter3() - GetParameter1();
        p = std::fmod(p, 100.0f);
        if (p < 0) p += 100.0f;
        SetParameter1(p);
    }
    else if (_type == "Square")
    {
        float p1 = GetParameter1();
        SetParameter1(GetParameter2());
        SetParameter2(p1);
    }
}

void ValueCurve::Flip()
{
    if (_type == "Custom")
    {
        for (auto& it : _values)
        {
            it.y = 1.0 - it.y;
            it.inDy = -it.inDy;
            it.outDy = -it.outDy;
        }
        if (_parameter1 == 1.0f) {
            _baseCustomValues = _values;
        }
    }
    else if (_type == "Ramp" || _type == "Saw Tooth" || _type == "Square" || _type == "Random")
    {
        SetParameter1(GetMax() - GetParameter1());
        SetParameter2(GetMax() - GetParameter2());
    }
    else if (_type == "Ramp Up/Down" || _type == "Ramp Up/Down Hold")
    {
        SetParameter1(GetMax() - GetParameter1());
        SetParameter2(GetMax() - GetParameter2());
        SetParameter3(GetMax() - GetParameter3());
    }
    else if (_type == "Flat")
    {
        SetParameter1(GetMax() - GetParameter1());
    }
    else if (_type == "Parabolic Down")
    {
        SetType("Parabolic Up");
        SetParameter2(GetMax() - GetParameter2());
    }
    else if (_type == "Parabolic Up")
    {
        SetType("Parabolic Down");
        SetParameter2(GetMax() - GetParameter2());
    }
    else if (_type == "Exponential Up")
    {
        SetType("Exponential Down");
    }
    else if (_type == "Exponential Down")
    {
        SetType("Exponential Up");
    }
    else if (_type == "Sine") {}
    else if (_type == "Logarithmic Up") {}
    else if (_type == "Logarithmic Down") {}
    else if (_type == "Music") {}
    else if (_type == "Inverted Music") {}
    else if (_type == "Music Trigger Fade") {}
    else if (_type == "Timing Track Toggle") {
    }
    else if (_type == "Timing Track Fade Fixed") {
    }
    else if (_type == "Timing Track Fade Proportional") {
    }
    else if (_type == "Decaying Sine") {}
    else if (_type == "Abs Sine") {}
    else { assert(false); }
}

// call this function from adjustSettings when a value curve has been changed to have a different divider ... it will convert the values to the equivalent and then you can serialise the value curve
void ValueCurve::ConvertDivider(int oldDivider, int newDivider)
{
    _parameter1 = (_parameter1 * (float)newDivider) / (float)oldDivider;
    _parameter2 = (_parameter2 * (float)newDivider) / (float)oldDivider;
    _parameter3 = (_parameter3 * (float)newDivider) / (float)oldDivider;
    _parameter4 = (_parameter4 * (float)newDivider) / (float)oldDivider;
    _min = (_min * (float)newDivider) / (float)oldDivider;
    _max = (_max * (float)newDivider) / (float)oldDivider;
    _viewMax = (_viewMax * (float)newDivider) / (float)oldDivider;

    // values are 0-1 normalised so dont need to be converted
}

float ValueCurve::Normalise(int parm, float value)
{
    float low;
    float high;
    ValueCurve::GetRangeParm(parm, _type, low, high);

    if (low == MINVOID)
    {
        assert(_min != MINVOIDF);
        low = _min;
    }
    if (high == MAXVOID)
    {
        assert(_max != MAXVOIDF);
        high = _max;
    }

    float res = value;
    if (low != 0 || high != 100)
    {
        float range = high - low;
        res = (value - low) * 100 / range;
    }

    if (res < 0) res = 0;
    if (res > 100) res = 100;

    return res;
}

// unfixes the changed scale from whatever it is now to 0-100
void ValueCurve::UnFixChangedScale(float newmin, float newmax)
{
    if (newmin == 0 && newmax == 100) return;

    float oldrange = newmax - newmin;
    float newrange = 100 - 0;
    float mindiff = -newmin * newrange / oldrange;

    float min, max;
    GetRangeParm(1, _type, min, max);
    if (min == MINVOID)
    {
        _parameter1 = _parameter1 * newrange / oldrange + mindiff;
    }

    GetRangeParm(2, _type, min, max);
    if (min == MINVOID)
    {
        _parameter2 = _parameter2 * newrange / oldrange + mindiff;
    }

    GetRangeParm(3, _type, min, max);
    if (min == MINVOID && (!IsStartEndLevelType(_type) || _hasStartEndLevel))
    {
        _parameter3 = _parameter3 * newrange / oldrange + mindiff;
    }

    GetRangeParm(4, _type, min, max);
    if (min == MINVOID && (!IsStartEndLevelType(_type) || _hasStartEndLevel))
    {
        _parameter4 = _parameter4 * newrange / oldrange + mindiff;
    }
}

// fixes curves that were saved with the wrong scale
void ValueCurve::FixScale(int scale)
{
    float min, max;
    GetRangeParm(1, _type, min, max);
    if (min == MINVOID)
    {
        _parameter1 *= scale;
    }

    GetRangeParm(2, _type, min, max);
    if (min == MINVOID)
    {
        _parameter2 *= scale;
    }

    GetRangeParm(3, _type, min, max);
    if (min == MINVOID)
    {
        _parameter3 *= scale;
    }

    GetRangeParm(4, _type, min, max);
    if (min == MINVOID)
    {
        _parameter4 *= scale;
    }
}

// fixes the changed scale from 0-100 to whatever it is now
void ValueCurve::FixChangedScale(float newmin, float newmax, int divisor)
{
    if (newmin == 0 && newmax == 100 && divisor == 1) return;

    float newrange = newmax - newmin;
    float oldrange = 100 - 0;
    float mindiff = newmin - 0;

    float min, max;
    GetRangeParm(1, _type, min, max);
    if (min == MINVOID)
    {
        _parameter1 = (_parameter1 * newrange / oldrange + mindiff); // / divisor;
    }

    GetRangeParm(2, _type, min, max);
    if (min == MINVOID)
    {
        _parameter2 = (_parameter2 * newrange / oldrange + mindiff); // / divisor;
    }

    GetRangeParm(3, _type, min, max);
    if (min == MINVOID && (!IsStartEndLevelType(_type) || _hasStartEndLevel))
    {
        _parameter3 = (_parameter3 * newrange / oldrange + mindiff); // / divisor;
    }

    GetRangeParm(4, _type, min, max);
    if (min == MINVOID && (!IsStartEndLevelType(_type) || _hasStartEndLevel))
    {
        _parameter4 = (_parameter4 * newrange / oldrange + mindiff); // / divisor;
    }
}

void ValueCurve::ConvertChangedScale(float newmin, float newmax)
{
    if (newmin == _min && newmax == _max) return;

    float newrange = newmax - newmin;
    float oldrange = _max - _min;
    // Rescale a real-value parameter from the current [_min, _max] range
    // into [newmin, newmax]:
    //   new = (old - _min) * newrange / oldrange + newmin
    // The previous formula ((old * newrange / oldrange) + (newmin - _min))
    // was only correct when _min == 0, per the original author's comment.
    // Shrinking ranges (newrange < oldrange) are also valid — the rescale
    // is linear in both directions.

    float min, max;
    GetRangeParm(1, _type, min, max);
    if (min == MINVOID) {
        _parameter1 = (_parameter1 - _min) * newrange / oldrange + newmin;
    }

    GetRangeParm(2, _type, min, max);
    if (min == MINVOID) {
        _parameter2 = (_parameter2 - _min) * newrange / oldrange + newmin;
    }

    GetRangeParm(3, _type, min, max);
    if (min == MINVOID && (!IsStartEndLevelType(_type) || _hasStartEndLevel)) {
        _parameter3 = (_parameter3 - _min) * newrange / oldrange + newmin;
    }

    GetRangeParm(4, _type, min, max);
    if (min == MINVOID && (!IsStartEndLevelType(_type) || _hasStartEndLevel)) {
        _parameter4 = (_parameter4 - _min) * newrange / oldrange + newmin;
    }

    // Custom curve points: rescale y values the same way.
    if (_type == "Custom")
    {
        assert(_min != MINVOIDF);
        assert(_max != MAXVOIDF);
        for (auto& it : _values)
        {
            it.y = (it.y - _min) * newrange / oldrange + newmin;
        }
        if (_parameter1 == 1.0f) {
            _baseCustomValues = _values;
        } else {
            ReconstructBaseCustomValues();
        }
    }
}

void ValueCurve::RenderType()
{
    // dont render if we dont know our limits
    if (_min == MINVOIDF || _max == MAXVOIDF || _divisor == MAXVOID) return;

    float parameter1 = Normalise(1, _parameter1);
    float parameter2 = Normalise(2, _parameter2);
    float parameter3 = Normalise(3, _parameter3);
    float parameter4 = Normalise(4, _parameter4);

    if (_type != "Custom" && !(_type == "Random" && _hasPreloadedValues))
    {
        _values.clear();
    }
    if (_type == "Flat")
    {
        _values.push_back(vcSortablePoint(0.0f, parameter1 / 100.0, false));
        _values.push_back(vcSortablePoint(1.0f, parameter1 / 100.0, false));
    }
    else if (_type == "Ramp")
    {
        _values.push_back(vcSortablePoint(0.0f, parameter1 / 100.0, false));
        _values.push_back(vcSortablePoint(1.0f, parameter2 / 100.0, false));
    }
    else if (_type == "Ramp Up/Down")
    {
        _values.push_back(vcSortablePoint(0.0f, parameter1 / 100.0, false));
        _values.push_back(vcSortablePoint(0.5f, parameter2 / 100.0, false));
        _values.push_back(vcSortablePoint(1.0f, parameter3 / 100.0, false));
    }
    else if (_type == "Ramp Up/Down Hold")
    {
        _values.push_back(vcSortablePoint(0.0f, parameter1 / 100.0, false));
        _values.push_back(vcSortablePoint(0.5f - ((0.5f * parameter3) / 100.0), parameter2 / 100.0, false));
        _values.push_back(vcSortablePoint(0.5f + ((0.5f * parameter3) / 100.0), parameter2 / 100.0, false));
        _values.push_back(vcSortablePoint(1.0f, parameter1 / 100.0, false));
    }
    else if (_type == "Saw Tooth")
    {
        int count = _parameter3;
        if (count < 1)
        {
            count = 1;
        }
        float per = 1.0f / count;
        _values.push_back(vcSortablePoint(0.0f, parameter1 / 100.0, false));
        for (int i = 0; i < count; i++)
        {
            _values.push_back(vcSortablePoint(i * per + per / 2.0f, parameter2 / 100.0, false));
            _values.push_back(vcSortablePoint((i + 1) * per, parameter1 / 100.0, false));
        }
    }
    else if (_type == "Music")
    {
        // ???
    }
    else if (_type == "Inverted Music")
    {
        // ???
    }
    else if (_type == "Music Trigger Fade")
    {
        // ???
    }
    else if (_type == "Timing Track Toggle") {
    }
    else if (_type == "Timing Track Fade Fixed") {
    }
    else if (_type == "Timing Track Fade Proportional") {
    }
    else if (_type == "Square")
    {
        int count = _parameter3;
        if (count < 1)
        {
            count = 1;
        }
        float per = 1.0f / (2 * count);
        bool low = true;

        for (int i = 0; i < 2 * count; i++)
        {
            float f1 = (float)i * per - 0.0001f;
            float f2 = (float)i * per;
            if (vcSortablePoint::Normalise(f1) != vcSortablePoint::Normalise(f2))
            {
                f1 = (float)i * per;
                f2 = (float)i * per + 0.0001f;
            }
            if (low)
            {
                if (i != 0)
                {
                    _values.push_back(vcSortablePoint(f1, (float)parameter2 / 100.0, false));
                }
                _values.push_back(vcSortablePoint(f2, (float)parameter1 / 100.0, false));
            }
            else
            {
                _values.push_back(vcSortablePoint(f1, (float)parameter1 / 100.0, false));
                _values.push_back(vcSortablePoint(f2, (float)parameter2 / 100.0, false));
            }
            low = !low;
        }
        _values.push_back(vcSortablePoint(1.0f, parameter2 / 100.0, false));
    }
    else if (_type == "Parabolic Down")
    {
        int a = _parameter1;
        if (_parameter1 == 0)
        {
            a = 1;
        }
        // y = a(x-h)^2 + bx + k
        // h = 0.5
        // k = 0.0
        for (double i = 0.0; i <= 1.01; i += 0.05)
        {
            if (i > 1.0) i = 1.0;
            float y = a * (i - 0.5f) * (i - 0.5f) + parameter2 / 100.0;
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            float fy = Safe01(y);
            if (_hasStartEndLevel) {
                float sn = parameter3 / 100.0f;
                float en = parameter4 / 100.0f;
                fy = Safe01(sn + fy * (en - sn));
            }
            _values.push_back(vcSortablePoint(i, fy, wrapped));
        }
    }
    else if (_type == "Parabolic Up")
    {
        int a = -1 * _parameter1;
        if (_parameter1 == 0)
        {
            a = -1;
        }
        // y = -1 * a(x-h)^2 + bx + k
        // h = 0.5
        // k = c
        for (double i = 0.0; i <= 1.01; i += 0.05)
        {
            if (i > 1.0) i = 1.0;
            float y = a * (i - 0.5f) * (i - 0.5f) + parameter2 / 100.0;
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            float fy = Safe01(y);
            if (_hasStartEndLevel) {
                float sn = parameter3 / 100.0f;
                float en = parameter4 / 100.0f;
                fy = Safe01(sn + fy * (en - sn));
            }
            _values.push_back(vcSortablePoint(i, fy, wrapped));
        }
    }
    else if (_type == "Logarithmic Up")
    {
        // p1 rate
        float a = parameter1 / 25.0f;
        if (_parameter1 == 0)
        {
            a = 0.04f;
        }
        // y = log(ax - 1);
        for (double i = 0.0; i <= 1.01; i += 0.05)
        {
            if (i > 1.0) i = 1.0;
            float y = (parameter2 - 50.0) / 50.0 + log(a + a*i) + 1.0f;
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            float fy = Safe01(y);
            if (_hasStartEndLevel) {
                float sn = parameter3 / 100.0f;
                float en = parameter4 / 100.0f;
                fy = Safe01(sn + fy * (en - sn));
            }
            _values.push_back(vcSortablePoint(i, fy, wrapped));
        }
    }
    else if (_type == "Logarithmic Down")
    {
        // p1 rate
        float a = parameter1 / 10.0f;
        if (_parameter1 == 0)
        {
            a = 0.1f;
        }
        // y = 1.0+ -2^(ax-1.0);
        for (double i = 0.0; i <= 1.01; i += 0.05)
        {
            if (i > 1.0) i = 1.0;
            float y = (parameter2 - 50.0) / 50.0 + 1.5f + -1 * exp2(a * i - 1.0f);
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            float fy = Safe01(y);
            if (_hasStartEndLevel) {
                float sn = parameter3 / 100.0f;
                float en = parameter4 / 100.0f;
                fy = Safe01(sn + fy * (en - sn));
            }
            _values.push_back(vcSortablePoint(i, fy, wrapped));
        }
    }
    else if (_type == "Exponential Up")
    {
        // p1 rate
        float a = parameter1 / 10.0f;
        if (_parameter1 == 0)
        {
            a = 0.1f;
        }
        // y = 1.0+ -2^(ax-1.0);
        for (double i = 0.0; i <= 1.01; i += 0.05)
        {
            if (i > 1.0) i = 1.0;
            float y = (parameter2 - 50.0) / 50.0 + (exp(a*i) - 1.0) / (exp(a) - 1.0);
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            float fy = Safe01(y);
            if (_hasStartEndLevel) {
                float sn = parameter3 / 100.0f;
                float en = parameter4 / 100.0f;
                fy = Safe01(sn + fy * (en - sn));
            }
            _values.push_back(vcSortablePoint(i, fy, wrapped));
        }
    }
    else if (_type == "Exponential Down")
    {
        // p1 rate
        float a = parameter1 / 10.0f;
        if (_parameter1 == 0)
        {
            a = 0.1f;
        }
        // y = 1.0+ -2^(ax-1.0);
        for (double i = 0.0; i <= 1.01; i += 0.05)
        {
            if (i > 1.0) i = 1.0;
            float y = (parameter2 - 50.0) / 50.0 + 1.0 - (exp(a*i) - 1.0) / (exp(a) - 1.0);
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            float fy = Safe01(y);
            if (_hasStartEndLevel) {
                float sn = parameter3 / 100.0f;
                float en = parameter4 / 100.0f;
                fy = Safe01(sn + fy * (en - sn));
            }
            _values.push_back(vcSortablePoint(i, fy, wrapped));
        }
    }
    else if (_type == "Sine")
    {
        // p1 - offset in cycle
        // p2 - maxy
        // p3 - cycles
        // one cycle = 2* PI
        static const double pi2 = 6.283185307;
        float maxx = pi2 * std::max(parameter3 / 10.0f, 0.1f);
        for (double i = 0.0; i <= 1.01; i += 0.025)
        {
            if (i > 1.0) i = 1.0;
            float r = i * maxx + ((parameter1 * pi2) / 100.0f);
            float y = (parameter4 - 50.0) / 50.0 + (sin(r) * (std::max(parameter2, 1.0f) / 200.0f)) + 0.5f;
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            _values.push_back(vcSortablePoint(i, Safe01(y), wrapped));
        }
    }
    else if (_type == "Decaying Sine")
    {
        // p1 - offset in cycle
        // p2 - maxy
        // p3 - cycles
        // one cycle = 2* PI
        static const double pi2 = 6.283185307;
        float maxx = pi2 * std::max((float)_parameter3 / 10.0f, 0.1f);
        for (double i = 0.0; i <= 1.01; i += 0.025)
        {
            if (i > 1.0) i = 1.0;

            float r = i * maxx + (((float)_parameter1 * pi2) / 100.0f);
            float exponent = exp(-0.1 * i * maxx);
            float y = ((float)_parameter4 - 50.0) / 50.0 + (exponent * cos(r) * (std::max((float)_parameter2, 1.0f) / 200.0f)) + 0.5f;
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    wrapped = true;
                    y -= 1.0f;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            _values.push_back(vcSortablePoint(i, Safe01(y), wrapped));
        }
    }
    else if (_type == "Random")
    {
        if (!_hasPreloadedValues)
        {
            // p1 - minimum
            // p2 - maximum
            // p3 - points
            float min = parameter1 / 100.0;
            float max = parameter2 / 100.0;
            int points = std::round(Denormalise(3, parameter3));

            // Deterministic stream seeded from the curve's identity — shapes
            // drawn from rand() made renders differ run to run.
            uint64_t rngSeed = 0xCBF29CE484222325ULL;
            auto seedMix = [&rngSeed](const void* d, size_t n) {
                const uint8_t* p = static_cast<const uint8_t*>(d);
                for (size_t i = 0; i < n; i++) {
                    rngSeed ^= p[i];
                    rngSeed *= 0x100000001B3ULL;
                }
            };
            seedMix(_id.data(), _id.size());
            seedMix(&_parameter1, sizeof(_parameter1));
            seedMix(&_parameter2, sizeof(_parameter2));
            seedMix(&_parameter3, sizeof(_parameter3));
            auto next01 = [&rngSeed]() {
                rngSeed += 0x9E3779B97F4A7C15ULL;
                uint64_t z = rngSeed;
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                z ^= z >> 31;
                return (z >> 11) * (1.0 / 9007199254740992.0);
            };

            if (points == 1)
            {
                float value = next01() * (max - min) + min;
                _values.push_back(vcSortablePoint(0.0f, value, false));
                _values.push_back(vcSortablePoint(1.0f, value, false));
            }
            else
            {
                float value = next01() * (max - min) + min;
                _values.push_back(vcSortablePoint(0.0f, value, false));
                value = next01() * (max - min) + min;
                _values.push_back(vcSortablePoint(1.0f, value, false));

                for (int i = 2; i < points; i++)
                {
                    float x = vcSortablePoint::Normalise(next01());
                    while (IsSetPoint(x))
                    {
                        x = vcSortablePoint::Normalise(next01());
                    }
                    value = next01() * (max - min) + min;
                    _values.push_back(vcSortablePoint(x, value, false));
                }
            }
        }
    }
    else if (_type == "Abs Sine")
    {
        // p1 - offset in cycle
        // p2 - maxy
        // p3 - cycles
        // one cycle = 2* PI
        static const double pi2 = 6.283185307;
        float maxx = pi2 * std::max(parameter3 / 10.0f, 0.1f);
        for (double i = 0.0; i <= 1.01; i += 0.025)
        {
            if (i > 1.0) i = 1.0;
            float r = i * maxx + ((parameter1 * pi2) / 100.0f);
            float y = (parameter4 - 50.0) / 50.0 + (std::abs(sin(r) * (std::max(parameter2, 1.0f) / 100.0f)));
            bool wrapped = false;
            if (_wrap)
            {
                while (y > 1.0f)
                {
                    y -= 1.0f;
                    wrapped = true;
                }
                while (y < 0.0f)
                {
                    wrapped = true;
                    y += 1.0f;
                }
            }
            _values.push_back(vcSortablePoint(i, Safe01(y), wrapped));
        }
    }
    else if (_type == "Custom")
    {
        if (_parameter1 < 1.0f) {
            _parameter1 = 1.0f;
        } else if (_parameter1 > 10.0f) {
            _parameter1 = 10.0f;
        }
        int cycles = std::round(_parameter1);
        if (cycles < 1) cycles = 1;
        if (cycles > 10) cycles = 10;

        if (cycles == 1) {
            if (_baseCustomValues.empty() && !_values.empty()) {
                _baseCustomValues = _values;
            } else if (!_baseCustomValues.empty()) {
                _values = _baseCustomValues;
            }
        } else {
            if (_baseCustomValues.empty() && !_values.empty()) {
                _baseCustomValues = _values;
            }
            _values.clear();
            for (int i = 0; i < cycles; ++i) {
                for (const auto& pt : _baseCustomValues) {
                    float newX = (pt.x + i) / (float)cycles;
                    vcSortablePoint np(newX, pt.y, pt.wrapped);
                    np.CopyShape(pt);
                    np.inDx /= cycles;
                    np.outDx /= cycles;
                    _values.push_back(np);
                }
            }
            _values.sort();
        }
    }
    _values.sort();
}

ValueCurve::ValueCurve(const std::string& id, float min, float max, const std::string type, float parameter1, float parameter2, float parameter3, float parameter4, bool wrap, float divisor, const std::string& timingTrack, const std::string& filterLabelText, bool isFilterLabelRegex)
{
    _type = type;
    _id = id;
    _min = min;
    _max = max;
    _wrap = wrap;
    _realValues = true;
    _divisor = divisor;
    _timingTrack = timingTrack;
    _filterLabelText = filterLabelText;
    _isFilterLabelRegex = isFilterLabelRegex;
    assert(_divisor == 1 || _divisor == 10 || _divisor == 100);
    _timeOffset = 0;
    _parameter1 = SafeParameter(1, parameter1);
    _parameter2 = SafeParameter(2, parameter2);
    _parameter3 = SafeParameter(3, parameter3);
    _parameter4 = SafeParameter(4, parameter4);
    _active = false;
    RenderType();
}

void ValueCurve::ClearCachedOffsets()
{
    _cachedTimingTrack.clear();
    _cachedStartMS = -1;
    _cachedEndMS = -1;
    _cachedOffsets.clear();
}

void ValueCurve::SetDefault(float min, float max, int divisor)
{
    ClearCachedOffsets();
    _type = "Flat";
    _baseCustomValues.clear();
    if (min != MINVOIDF)
    {
        _min = min;
    }
    if (max != MAXVOIDF)
    {
        _max = max;
    }
    _timingTrack = "";
    _audioTrackName = "";
    _filterLabelText = "";
    _isFilterLabelRegex = false;
    _viewMax = 0.0f;
    _colorKeys.clear();
    _parameter1 = 0;
    _parameter2 = 0;
    _parameter3 = 0;
    _parameter4 = 0;
    _timeOffset = 0;
    _active = false;
    _wrap = false;
    _realValues = true;
    _hasStartEndLevel = false;
    if (divisor != MAXVOID)
    {
        _divisor = divisor;
    }
    assert(_divisor == 1 || _divisor == 10 || _divisor == 100);

    RenderType();
}

ValueCurve::ValueCurve(const std::string& s)
{
    _min = MINVOIDF;
    _max = MAXVOIDF;
    _divisor = 1;
    _timingTrack = "";
    _audioTrackName = "";
    _filterLabelText = "";
    _isFilterLabelRegex = false;
    SetDefault();
    Deserialise(s);
}

namespace {
std::string EscapePipe(std::string s) {
    Replace(s, "|", "&#124;");
    return s;
}

std::string UnescapePipe(std::string s) {
    Replace(s, "&#124;", "|");
    return s;
}
}

void ValueCurve::Deserialise(const std::string& s, bool holdminmax)
{
    ClearCachedOffsets();
    if (s == "")
    {
        SetDefault(0, 100);
        _active = false;
    }
    else if (s.find('|') == std::string::npos)
    {
        SetDefault(0, 100);
        _active = false;
        _id = s;
    }
    else
    {
        _realValues = false;
        _active = true;
        _values.clear();
        _baseCustomValues.clear();
        _hasPreloadedValues = false;
        _hasStartEndLevel = false;
        _type = "Flat";
        _parameter1 = 0.0f;
        _parameter2 = 0.0f;
        _parameter3 = 0.0f;
        _parameter4 = 0.0f;
        _timeOffset = 0;
        _wrap = false;
        _timingTrack = "";
        _filterLabelText = "";
        _isFilterLabelRegex = false;
        _viewMax = 0.0f;
        _colorKeys.clear();

        float oldmin = _min;
        float oldmax = _max;

        std::string parse = s;
        while (!parse.empty()) {
            size_t idx = parse.find('|');
            std::string vs = (idx != std::string::npos) ? parse.substr(0, idx) : parse;
            if (idx != std::string::npos) {
                parse.erase(0, idx + 1);
            } else {
                parse.clear();
            }
            idx = vs.find('=');
            if (idx != std::string::npos) {
                SetSerialisedValue(vs.substr(0, idx), vs.substr(idx + 1));
            }
        }

        // if we are asked to hold min max then we are just going to use the ones we deserialised and assume parms are all ok
        // this is generally only done when loading an xvc file ... particularly an old one
        if (!holdminmax)
        {
            // This converts curves from the 0-100 to the real scale
            if (_active && !_realValues)
            {
                if (_min == 0 && _max == 100 && _divisor == 1)
                {
                    // no rescaling needed
                    _realValues = true;
                }
                else if (oldmin != MINVOIDF)
                {
                    if (_min != 0 || _max != 100)
                    {
                        // use the scale in the file if it wasnt 0-100
                        FixChangedScale(_min, _max, _divisor);
                    }
                    else
                    {
                        if (oldmin != 0 || oldmax != 100 || _divisor != 1)
                        {
                            // otherwise use the scale of this VC ... this is not great ... if the VC range has been expanded then
                            // it isnt going to convert correctly

                            // this is actually checking for something 2018.23 or newer
                            // this should be updated every release by 1 until we decide to change a slider range for the first time
                            // at that point we are going to need to force people to go back to a version after 2017.24 but before the
                            // first version with the change
                            if (!::IsVersionOlder("2018.46", xlights_version_string))
                            {
                                static std::string warnedfile = "";

                                std::string seqName = __sequenceElements ? __sequenceElements->GetRenderContext()->GetShowDirectory() : "";
                                if (!seqName.empty() && warnedfile != seqName)
                                {
                                    warnedfile = seqName;
                                    DisplayWarning("Sequence contains value curves that cannot be converted automatically. Please open and save this sequence in v2018.23 before proceeding.");
                                }
                            }
                            FixChangedScale(oldmin, oldmax, _divisor);
                        }
                    }
                    _realValues = true;
                }
            }

            // This converts curves to the new scale when a parameters range has been expanded ... but only if it was already real values
            if (_active && _realValues && ((oldmin != MINVOIDF && oldmin != _min) || (oldmax != MAXVOIDF && oldmax != _max)))
            {
                ConvertChangedScale(oldmin, oldmax);
            }

            if (oldmin != MINVOIDF) _min = oldmin;
            if (oldmax != MAXVOIDF) _max = oldmax;
        }

        if (_type == "Custom")
        {
            ReconstructBaseCustomValues();
        }

        RenderType();
    }
}

std::string ValueCurve::Serialise()
{
    std::string res = "";

    if (IsActive())
    {
        assert(_min != MINVOIDF);
        assert(_max != MAXVOIDF);

        res += "Active=TRUE|";
        res += "Id=" + _id + "|";
        if (_type != "Flat")
        {
            res += "Type=" + _type + "|";
        }
        res += "Min=" + fmtVC(_min) + "|";
        res += "Max=" + fmtVC(_max) + "|";
        if (!_colorKeys.empty()) {
            // x:RRGGBB:B (blend) or :H (hard), ; between keys
            res += "Colors=";
            for (size_t i = 0; i < _colorKeys.size(); ++i) {
                const auto& k = _colorKeys[i];
                char hex[8];
                std::snprintf(hex, sizeof(hex), "%02X%02X%02X", k.color.red, k.color.green, k.color.blue);
                res += (i ? ";" : "") + fmtVC(k.x) + ":" + hex + ":" + (k.blend ? "B" : "H");
            }
            res += "|";
        }
        if (HasViewMax()) {
            res += "VMax=" + fmtVC(_viewMax) + "|";
        }
        if (_timingTrack != "") {
            res += "TT=" + EscapePipe(_timingTrack) + "|";
        }
        if (_audioTrackName != "") {
            res += "AT=" + EscapePipe(_audioTrackName) + "|";
        }
        if (_filterLabelText != "") {
            res += "FT=" + EscapePipe(_filterLabelText) + "|";
        }
        if (_isFilterLabelRegex) {
            res += "FLR=Y|";
        }
        if (_parameter1 != 0)
        {
            res += "P1=" + fmtVC(_parameter1) + "|";
        }
        if (_parameter2 != 0)
        {
            res += "P2=" + fmtVC(_parameter2) + "|";
        }
        if (_hasStartEndLevel)
        {
            res += "SE=Y|";
            res += "P3=" + fmtVC(_parameter3) + "|";
            res += "P4=" + fmtVC(_parameter4) + "|";
        }
        else
        {
            if (_parameter3 != 0)
            {
                res += "P3=" + fmtVC(_parameter3) + "|";
            }
            if (_parameter4 != 0)
            {
                res += "P4=" + fmtVC(_parameter4) + "|";
            }
        }
        if (_timeOffset != 0)
        {
            res += "TO=" + std::to_string(_timeOffset) + "|";
        }
        if (_wrap)
        {
            res += "WRAP=TRUE|";
        }
        if (_realValues)
        {
            res += "RV=TRUE|";
        }
        if (_type == "Custom")
        {
            RemoveExcessCustomPoints();
            res += "Values=";
            for (auto it = _values.begin(); it != _values.end(); ++it)
            {
                // x:y, then :E for an eased point or :C:inDx:inDy:outDx:outDy for a custom one (readers that only
                // know x:y stop at the second colon)
                res += fmtVC(it->x) + ":" + fmtVC(it->y);
                if (it->shape == vcSortablePoint::Shape::Ease) {
                    res += ":E";
                } else if (it->shape == vcSortablePoint::Shape::Custom) {
                    res += ":C:" + fmtVC(it->inDx) + ":" + fmtVC(it->inDy) + ":" + fmtVC(it->outDx) + ":" + fmtVC(it->outDy);
                }
                if (std::next(it) != _values.end())
                {
                    res += ";";
                }
            }
            res += "|";
        }
        else if (_type == "Random")
        {
            // Persist the generated random values so they survive deserialization
            // without being re-randomized on every frame
            res += "Values=";
            for (auto it = _values.begin(); it != _values.end(); ++it)
            {
                res += fmtVC(it->x) + ":" + fmtVC(it->y);
                if (std::next(it) != _values.end())
                {
                    res += ";";
                }
            }
            res += "|";
        }
    }
    else
    {
        res += "Active=FALSE|";
    }
    return res;
}

void ValueCurve::LoadXVC(const std::string& fn)
{
    if (!FileExists(fn)) {
        DisplayError("Failure loading value curve file " + fn + ".");
        return;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(fn.c_str());

    if (result)
    {
        pugi::xml_node root = doc.document_element();

        if (root && std::string_view(root.name()) == "valuecurve")
        {
            std::string data = root.attribute("data").as_string();

            // Add any valuecurve version conversion logic here
            // Source version will be the program version that created the custom model

            Deserialise(data, true);

            if (GetId() == "ID_VALUECURVE_XVC")
            {
                // this should already have the 0-100 scale
            }
            else
            {
                // need to fudge it
                float min = GetMin();
                float max = GetMax();
                SetLimits(0, 100);
                FixChangedScale(min, max, 1);
            }

            SetActive(true);
            if (_type == "Custom")
            {
                ReconstructBaseCustomValues();
            }
        }
        else
        {
            DisplayError("Failure loading value curve file " + fn + ".");
        }
    }
    else
    {
        DisplayError("Failure loading value curve file " + fn + ".");
    }
}

void ValueCurve::SaveXVC(const std::string& filename)
{
    spdlog::info("Saving to xvc file {}.", filename);

    std::ofstream f(filename);
    if (!f.is_open())
    {
        DisplayError("Unable to create file " + filename + ". " + strerror(errno) + "\n");
        return;
    }

    bool active = IsActive();
    std::string id = GetId();
    if (id == "") SetId("Dummy");
    SetActive(true);

    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<valuecurve \n";
    ValueCurve vc(Serialise());
    vc.SetId("ID_VALUECURVE_XVC");
    vc.SetLimits(0, 100);
    vc.UnFixChangedScale(GetMin(), GetMax());
    f << "data=\"" << vc.Serialise() << "\" ";
    f << "SourceVersion=\"" << xlights_version_string << "\" ";
    f << " >\n";
    f << "</valuecurve>";
    f.close();
    SetActive(active);
    SetId(id);
}

void ValueCurve::SetSerialisedValue(const std::string &k, const std::string &s)
{
    if (k == "Id") {
        _id = s;
    } else if (k == "Active") {
        if (s == "FALSE") {
            _active = false;
        } else {
            // it should already be true
            assert(_active == true);
        }
    } else if (k == "Type") {
        _type = s;
    }
    else if (k == "TT") {
        _timingTrack = UnescapePipe(s);
    } else if (k == "AT") {
        _audioTrackName = UnescapePipe(s);
    } else if (k == "FT") {
        _filterLabelText = UnescapePipe(s);
    } else if (k == "FLR") {
        _isFilterLabelRegex = (s == "Y");
    } else if (k == "Min") {
        _min = std::strtof(s.c_str(), nullptr);
    } else if (k == "Max") {
        _max = std::strtof(s.c_str(), nullptr);
    } else if (k == "VMax") {
        _viewMax = std::strtof(s.c_str(), nullptr);
    } else if (k == "Colors") {
        std::vector<vcColorKey> keys;
        size_t start = 0;
        while (start < s.size()) {
            size_t end = s.find(';', start);
            std::string item = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
            size_t c1 = item.find(':');
            size_t c2 = c1 == std::string::npos ? std::string::npos : item.find(':', c1 + 1);
            if (c1 != std::string::npos) {
                vcColorKey key;
                key.x = std::strtof(item.substr(0, c1).c_str(), nullptr);
                unsigned long rgb = std::strtoul(item.substr(c1 + 1, c2 == std::string::npos ? std::string::npos : c2 - c1 - 1).c_str(), nullptr, 16);
                key.color = xlColor((uint8_t)((rgb >> 16) & 0xFF), (uint8_t)((rgb >> 8) & 0xFF), (uint8_t)(rgb & 0xFF));
                key.blend = c2 == std::string::npos || item.substr(c2 + 1) != "H";
                keys.push_back(key);
            }
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
        SetColorKeys(std::move(keys));
    } else if (k == "P1") {
        _parameter1 = std::strtof(s.c_str(), nullptr);
    } else if (k == "TO") {
        _timeOffset = std::strtol(s.c_str(), nullptr, 10);
    } else if (k == "WRAP") {
        _wrap = true;
    } else if (k == "RV") {
        _realValues = true;
    } else if (k == "P2") {
        _parameter2 = std::strtof(s.c_str(), nullptr);
    } else if (k == "SE") {
        _hasStartEndLevel = true;
    } else if (k == "P3") {
        _parameter3 = std::strtof(s.c_str(), nullptr);
    } else if (k == "P4") {
        _parameter4 = std::strtof(s.c_str(), nullptr);
    } else if (k == "Values") {
        std::string parse = s;
        while (!parse.empty()) {
            size_t idx = parse.find(';');
            std::string vs = (idx != std::string::npos) ? parse.substr(0, idx) : parse;
            if (idx != std::string::npos) {
                parse.erase(0, idx + 1);
            } else {
                parse.clear();
            }
            idx = vs.find(':');
            if (idx != std::string::npos) {
                std::vector<std::string> f;
                size_t start = 0;
                while (true) {
                    size_t c = vs.find(':', start);
                    f.push_back(vs.substr(start, c == std::string::npos ? std::string::npos : c - start));
                    if (c == std::string::npos) {
                        break;
                    }
                    start = c + 1;
                }
                float x = std::strtof(f[0].c_str(), nullptr);
                float y = std::strtof(f[1].c_str(), nullptr);

                vcSortablePoint pt(x, y, false);
                if (f.size() >= 3 && f[2] == "E") {
                    pt.shape = vcSortablePoint::Shape::Ease;
                } else if (f.size() >= 7 && f[2] == "C") {
                    pt.shape = vcSortablePoint::Shape::Custom;
                    pt.inDx = std::strtof(f[3].c_str(), nullptr);
                    pt.inDy = std::strtof(f[4].c_str(), nullptr);
                    pt.outDx = std::strtof(f[5].c_str(), nullptr);
                    pt.outDy = std::strtof(f[6].c_str(), nullptr);
                }
                _values.push_back(pt);
            }
        }
        if (!_values.empty()) {
            _hasPreloadedValues = true;
        }
    }

    _values.sort();
}

void ValueCurve::SetType(std::string type)
{
    _hasPreloadedValues = false;
    if (type != "Custom" && _type != type) {
        _baseCustomValues.clear();
    }
    _type = type;
    RenderType();
}

float ValueCurve::GetScaledValue(float offset) const
{
    assert(_min != MINVOIDF);
    assert(_max != MAXVOIDF);
    return (_min + (_max - _min) * offset) / _divisor;
}

float ValueCurve::GetOutputValueAt(float offset, long startMS, long endMS)
{
    assert(_min != MINVOIDF);
    assert(_max != MAXVOIDF);
    return _min + (_max - _min) * GetValueAt(offset, startMS, endMS);
}

float ValueCurve::GetOutputValueAtDivided(float offset, long startMS, long endMS)
{
    assert(_min != MINVOIDF);
    assert(_max != MAXVOIDF);
    return (_min + (_max - _min) * GetValueAt(offset, startMS, endMS)) / _divisor;
}

float ValueCurve::GetMaxValueDivided()
{
    float max = GetMin() / _divisor;
    for (int i = 0; i < 100; ++i)
    {
        max = std::max(max, GetOutputValueAtDivided(i * 50, 0, 100 * 50));
    }
    return max;
}

float ValueCurve::ApplyGain(float value, int gain) const
{
    float v = (100.0 + gain) * value / 100.0;
    if (v > 1.0) v = 1.0;
    return v;
}

int ValueCurve::GetPriorTimingMark(const std::string& timingTrack, int time, bool startsOnly, const std::string& filterLabelText, bool isFilterLabelRegex) {
    auto te = __sequenceElements->GetTimingElement(timingTrack);
    if (te != nullptr) {
        auto el = te->GetEffectLayer(0);
        if (el != nullptr) {
            if (startsOnly) {
                auto e = el->GetEffectBeforeTime(time, filterLabelText, isFilterLabelRegex);
                if (e != nullptr) {
                    return e->GetStartTimeMS();
                }
            }
            else {
                auto e = el->GetEffectBeforeTime(time, filterLabelText, isFilterLabelRegex);
                if (e != nullptr) {
                    if (e->GetEndTimeMS() < time) {
                        return e->GetEndTimeMS();
                    }
                    else {
                        return e->GetStartTimeMS();
                    }
                }
            }
        }
    }

    return -1;
}

int ValueCurve::GetSubsequentTimingMark(const std::string& timingTrack, int time, bool startsOnly, const std::string& filterLabelText, bool isFilterLabelRegex) {
    auto te = __sequenceElements->GetTimingElement(timingTrack);
    if (te != nullptr) {
        auto el = te->GetEffectLayer(0);
        if (el != nullptr) {
            if (startsOnly) {
                auto e = el->GetEffectAfterTime(time, filterLabelText, isFilterLabelRegex);
                if (e != nullptr) {
                    return e->GetStartTimeMS();
                }
            }
            else {
                auto e = el->GetEffectAtTime(time, filterLabelText, isFilterLabelRegex);
                if (e != nullptr) {
                    return e->GetEndTimeMS();
                }
                else {
                    auto e = el->GetEffectAfterTime(time, filterLabelText, isFilterLabelRegex);
                    if (e != nullptr) {
                        return e->GetStartTimeMS();
                    }
                }
            }
        }
    }
    return -1;
}

std::vector<double> ValueCurve::GetTimingMarkOffsets(long startMS, long endMS) const {
    std::vector<double> offsets;
    if (_timingTrack.empty()) {
        return offsets;
    }
    if (__sequenceElements == nullptr) {
        return offsets;
    }
    auto te = __sequenceElements->GetTimingElement(_timingTrack);
    if (te == nullptr) {
        return offsets;
    }
    auto el = te->GetEffectLayer(0);
    if (el == nullptr) {
        return offsets;
    }

    offsets.push_back(0.0);
    for (int i = 0; i < el->GetEffectCount(); ++i) {
        Effect* e = el->GetEffect(i);
        long tStart = e->GetStartTimeMS();
        long tEnd = e->GetEndTimeMS();
        if (tStart > startMS && tStart < endMS) {
            offsets.push_back((double)(tStart - startMS) / (endMS - startMS));
        }
        if (tEnd > startMS && tEnd < endMS) {
            offsets.push_back((double)(tEnd - startMS) / (endMS - startMS));
        }
    }
    offsets.push_back(1.0);

    std::sort(offsets.begin(), offsets.end());
    offsets.erase(std::unique(offsets.begin(), offsets.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-5;
    }), offsets.end());

    return offsets;
}

std::vector<double> ValueCurve::GetTimingMarkOffsetsCached(long startMS, long endMS) {
    if (_cachedTimingTrack != _timingTrack || _cachedStartMS != startMS || _cachedEndMS != endMS) {
        _cachedOffsets = GetTimingMarkOffsets(startMS, endMS);
        _cachedTimingTrack = _timingTrack;
        _cachedStartMS = startMS;
        _cachedEndMS = endMS;
    }
    return _cachedOffsets;
}

std::vector<double> ValueCurve::GetResampledOffsets(long startMS, long endMS, int cycles) {
    if (cycles < 1) cycles = 1;
    if (cycles > 10) cycles = 10;
    std::vector<double> offsets = GetTimingMarkOffsetsCached(startMS, endMS);
    std::vector<double> resampled(cycles + 1, 0.0);
    resampled[cycles] = 1.0;
    if (offsets.size() < 2) {
        for (int i = 1; i < cycles; ++i) {
            resampled[i] = (double)i / cycles;
        }
        return resampled;
    }
    for (int i = 1; i < cycles; ++i) {
        double target = (double)i / cycles;
        auto it = std::lower_bound(offsets.begin(), offsets.end(), target);
        double val = 0.0;
        if (it == offsets.end()) {
            val = offsets.back();
        } else if (it == offsets.begin()) {
            val = offsets.front();
        } else {
            double val1 = *it;
            double val2 = *(it - 1);
            if (std::abs(val1 - target) < std::abs(val2 - target)) {
                val = val1;
            } else {
                val = val2;
            }
        }
        resampled[i] = std::max(resampled[i - 1], std::min(1.0, val));
    }
    return resampled;
}

float ValueCurve::GetValueAt(float offset, long startMS, long endMS)
{
    float res = 0.0f;

    // Resolve which audio manager to use for this curve
    AudioManager* _am = _audioTrackName.empty() ? __audioManager : GetAltAudio(_audioTrackName);

    // "Music Trigger Fade" is the one type that lazily populates _values inside
    // GetValueAt (it needs the audio manager, unavailable at setup). That
    // push_back - and the point interpolation that reads _values later in this
    // same call - is a data race when GetValueAt runs concurrently on the same
    // curve (e.g. the parallel gap-fill in PixelBufferClass::GetMixedColor, or
    // any effect that evaluates a value curve inside its own parallel_for).
    // Serialize this type; held for the whole call so populate + read are
    // consistent. Other types either set res directly or read a _values list
    // that was populated serially in RenderType(), so they need no lock.
    std::unique_lock<std::mutex> mtfLock;
    if (_type == "Music Trigger Fade") {
        static std::mutex mtfMutex;
        mtfLock = std::unique_lock<std::mutex>(mtfMutex);
    }

    // If we are music trigger fade and we dont have values ... calculate them on the fly
    if (_type == "Music Trigger Fade") {
        // Just generate what we need on the fly
        if (_am != nullptr && _values.size() == 0) {
            float min = (GetParameter1() - _min) / (_max - _min);
            float max = (GetParameter2() - _min) / (_max - _min);
            int frameMS = _am->GetFrameInterval();
            int fadeFrames = GetParameter4();
            float yperFrame = (max - min) / fadeFrames;
            float perPoint = vcSortablePoint::perPoint();
            int msperPoint = perPoint * (endMS - startMS);
            if (msperPoint < frameMS) msperPoint = frameMS;
            if (yperFrame <= 0) yperFrame = 0.0001f;

            // build what is esentially a sawtooth curve which peaks when audio exceeds the level or drops when it doesnt.
            // this could hpothetically create 2 points per frame

            float runningy = min;

            // this just ensures the curve is complete
            _values.push_back(vcSortablePoint(0, min, _wrap));

            for (long time = startMS; time < endMS; time += msperPoint) {

                float x = vcSortablePoint::Normalise((float)(time - startMS) / (float)(endMS - startMS));
                float prex = vcSortablePoint::Normalise((float)(time - startMS - msperPoint) / (float)(endMS - startMS));

                // find the maximum of any intervening frames
                float f = 0.0;
                for (long ms = time; ms < time + msperPoint; ms += frameMS) {
                    auto pf = _am->GetFrameData("", ms + frameMS);
                    if (pf != nullptr) {
                        if (pf->max > f) {
                            f = pf->max;
                        }
                    }
                }

                if (f * 100.0 > GetParameter3()) {
                    if (time == startMS) {
                        runningy = max;
                        _values.back().y = runningy;
                    }
                    else if (runningy != max) {
                        _values.push_back(vcSortablePoint(x, runningy, _wrap));
                        runningy = max;
                        _values.push_back(vcSortablePoint(x, runningy, _wrap));
                    }
                }
                else {
                    if (runningy <= min) {
                        // do nothing
                        runningy = min;
                    }
                    else {
                        if (runningy == max) {
                            if (_values.back().x < prex || _values.back().y != runningy) {
                                _values.push_back(vcSortablePoint(prex, runningy, _wrap));
                            }
                        }
                        runningy -= yperFrame;
                        if (runningy <= min) {
                            runningy = min;
                            _values.push_back(vcSortablePoint(x, runningy, _wrap));
                        }
                    }
                }
            }
            // this just ensures the curve is complete
            _values.push_back(vcSortablePoint(1.0, runningy, _wrap));
        }
    }

    if (_type == "Timing Track Toggle") {
        float min = (GetParameter1() - _min) / (_max - _min);
        float max = (GetParameter2() - _min) / (_max - _min);
        bool up = false;
        long time = (float)startMS + offset * (endMS - startMS);
        int next = GetSubsequentTimingMark(_timingTrack, startMS - 1, true, _filterLabelText, _isFilterLabelRegex);
        if (next != -1) {
            while (next != -1 && next <= time) {
                up = !up;
                next = GetSubsequentTimingMark(_timingTrack, next + 1, false, _filterLabelText, _isFilterLabelRegex);
            }
        }

        if (up) {
            res = max;
        }
        else {
            res = min;
        }
    }
    else if (_type == "Timing Track Fade Fixed") {
        float min = (GetParameter1() - _min) / (_max - _min);
        float max = (GetParameter2() - _min) / (_max - _min);
        long time = (float)startMS + offset * (endMS - startMS);
        int prior = GetPriorTimingMark(_timingTrack, time + 1, true, _filterLabelText, _isFilterLabelRegex);
        if (prior == -1) {
            res = min;
        }
        else {
            int frame = (time - prior) / __sequenceElements->GetFrameMS();
            int fadeFrames = GetParameter3();
            if (frame < fadeFrames) {
                res = min + (max - min) * (((float)fadeFrames - (float)frame) / (float)fadeFrames);
            }
            else {
                res = min;
            }
        }
    }
    else if (_type == "Timing Track Fade Proportional") {
        float min = (GetParameter1() - _min) / (_max - _min);
        float max = (GetParameter2() - _min) / (_max - _min);
        long time = (float)startMS + offset * (endMS - startMS);
        int prior = GetPriorTimingMark(_timingTrack, time + 1, true, _filterLabelText, _isFilterLabelRegex);
        if (prior == -1) {
            res = min;
        }
        else {
            int next = GetSubsequentTimingMark(_timingTrack, time + 1, false, _filterLabelText, _isFilterLabelRegex);
            if (next == -1) {
                res = min;
            }
            else {
                int frame = (time - prior) / __sequenceElements->GetFrameMS();
                int fadeFrames = (((next - prior) / __sequenceElements->GetFrameMS()) * GetParameter3()) / 100;
                if (frame < fadeFrames) {
                    res = min + (max - min) * (((float)fadeFrames - (float)frame) / (float)fadeFrames);
                }
                else {
                    res = min;
                }
            }
        }
    }
    else if (_type == "Music" || _type == "Inverted Music") {
        if (_am != nullptr) {
            long time = (float)startMS + offset * (endMS - startMS);
            float f = 0.0;
            auto pf = _am->GetFrameData("", time);
            if (pf != nullptr) {
                f = ApplyGain(pf->max, GetParameter3());
                if (_type == "Inverted Music") {
                    f = 1.0 - f;
                }
            }

            float min = (GetParameter1() - _min) / (_max - _min);
            float max = (GetParameter2() - _min) / (_max - _min);
            res = min + f * (max - min);
        }
    }
    else {
        if (_values.size() < 2) return 1.0f;
        if (!_active) return 1.0f;

        if (offset < 0.0f) offset = 0.0;
        if (offset > 1.0f) offset = 1.0;

        float alignedOffset = offset;
        if (_type == "Custom" && _parameter1 > 1.0f && !_timingTrack.empty()) {
            int cycles = std::round(_parameter1);
            if (cycles < 1) cycles = 1;
            if (cycles > 10) cycles = 10;
            if (cycles > 1) {
                std::vector<double> resampledOffsets = GetResampledOffsets(startMS, endMS, cycles);
                if (resampledOffsets.size() > 1) {
                    int j = 0;
                    auto it = std::upper_bound(resampledOffsets.begin(), resampledOffsets.end(), (double)offset);
                    if (it == resampledOffsets.end()) {
                        j = (int)resampledOffsets.size() - 2;
                    } else {
                        j = std::distance(resampledOffsets.begin(), it) - 1;
                        if (j < 0) j = 0;
                    }
                    double o_j = resampledOffsets[j];
                    double o_j1 = resampledOffsets[j+1];
                    double frac = 0.0;
                    if (o_j1 > o_j) {
                        frac = (offset - o_j) / (o_j1 - o_j);
                    }
                    alignedOffset = (j + frac) / (float)cycles;
                    if (alignedOffset > 1.0f) alignedOffset = 1.0f;
                }
            }
        }

        float queryOffset = alignedOffset + (float)_timeOffset / 100;
        if (queryOffset > 1.0) queryOffset -= 1.0;
        if (queryOffset < 0.0) queryOffset = 0.0;

        vcSortablePoint last = _values.front();
        auto it = _values.begin();
        ++it;

        while (it != _values.end() && it->x < queryOffset) {
            last = *it;
            ++it;
        }

        if (it == _values.end()) {
            res = _values.back().y;
        }
        else if (it->x == last.x) {
            // this should not be possible
            res = it->y;
        }
        else {
            if (it->x == queryOffset) {
                res = it->y;
            }
            else if (it->IsWrapped() != last.IsWrapped()) {
                res = it->y;
            }
            else {
                res = SegmentValue(last, *it, queryOffset);
            }
        }
    }

    if (res < 0.0f) {
        res = 0.0f;
    }
    if (res > 1.0f) {
        res = 1.0f;
    }

    return res;
}

bool ValueCurve::IsSetPoint(float offset)
{
    auto it = _values.begin();
    while (it != _values.end() && *it <= offset)
    {
        if (*it == offset)
        {
            return true;
        }
        ++it;
    }

    return false;
}

void ValueCurve::DeletePoint(float offset)
{
    if (GetPointCount() > 2)
    {
        auto it = _values.begin();
        while (it != _values.end() && *it <= offset)
        {
            if (*it == offset)
            {
                _values.remove(*it);
                break;
            }
            ++it;
        }
    }

    if (_type == "Custom" && _parameter1 == 1.0f) {
        _baseCustomValues = _values;
    }
}

void ValueCurve::RemoveExcessCustomPoints()
{
    if (_values.size() < 3)
        return;
    // go through list and remove middle points where 3 in a row have the same value
    auto it1 = _values.begin();
    auto it2 = it1;
    ++it2;
    auto it3 = it2;
    ++it3;

    while (it1 != _values.end() && it2 != _values.end() && it3 != _values.end())
    {
        if (it1->y == it2->y && it2->y == it3->y && it1->IsHard() && it2->IsHard() && it3->IsHard())
        {
            _values.erase(it2);
            it2 = it1;
            ++it2;
            it3 = it2;
            ++it3;
        }
        else
        {
            ++it1;
            ++it2;
            ++it3;
        }
    }
}

void ValueCurve::SetValueAt(float offset, float value, bool force)
{
    vcSortablePoint np(offset, value, false);
    auto it = _values.begin();
    while (it != _values.end() && *it <= offset)
    {
        if (*it == offset && !force)
        {
            np.CopyShape(*it); // a new value for the same point keeps its shape
            _values.remove(*it);
            break;
        }
        ++it;
    }
    _values.push_back(np);
    _values.sort();

    if (_type == "Custom" && _parameter1 == 1.0f) {
        _baseCustomValues = _values;
    }
}

void ValueCurve::SetWrap(bool wrap)
{
    _wrap = wrap;

    if (!_wrap)
    {
        for (auto it = _values.begin(); it != _values.end(); ++it)
        {
            it->ClearWrap();
        }
    }

    RenderType();
}

float ValueCurve::GetParameter1_100() const
{
    float range = _max - _min;
    return (GetParameter1() - _min) * 100 / range;
}

float ValueCurve::FindMinPointLessThan(float point) const
{
    float res = 0.0;

    for (const auto& it : _values)
    {
        if (it.x < point)
        {
            res = it.x + (1.0 / VC_X_POINTS);
        }
    }

    return vcSortablePoint::Normalise(res);
}

float ValueCurve::FindMaxPointGreaterThan(float point) const
{
    float res = 1.0;

    for (const auto& it : _values)
    {
        if (it.x > point)
        {
            res = it.x - (1.0 / VC_X_POINTS);
            break;
        }
    }

    return vcSortablePoint::Normalise(res);
}

bool ValueCurve::NearCustomPoint(float x, float y) const
{
    for (const auto& it : _values) {
        if (it.IsNear(x, y)) {
            return true;
        }
    }

    return false;
}

void ValueCurve::SetColorKeys(std::vector<vcColorKey> keys)
{
    for (auto& k : keys) {
        k.x = std::clamp(k.x, 0.0f, 1.0f);
    }
    std::stable_sort(keys.begin(), keys.end(), [](const vcColorKey& a, const vcColorKey& b) { return a.x < b.x; });
    _colorKeys = std::move(keys);
}

xlColor ValueCurve::GetColorAt(float offset) const
{
    if (_colorKeys.empty()) {
        return xlBLACK;
    }
    if (_colorKeys.size() == 1) {
        return _colorKeys.front().color; // it goes from itself to itself: solid throughout, Blend or Hard
    }
    const float o = std::clamp(offset, 0.0f, 1.0f);
    auto mix = [](const xlColor& a, const xlColor& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return xlColor((uint8_t)std::lround(a.red + (b.red - a.red) * t),
                       (uint8_t)std::lround(a.green + (b.green - a.green) * t),
                       (uint8_t)std::lround(a.blue + (b.blue - a.blue) * t));
    };
    const vcColorKey& first = _colorKeys.front();
    const vcColorKey& last = _colorKeys.back();
    const bool loop = _wrap && first.blend;

    size_t next = 0;
    while (next < _colorKeys.size() && _colorKeys[next].x <= o) {
        ++next;
    }
    if (next == 0) {
        // before the first key
        if (loop) {
            const float from = last.x - 1.0f;
            return mix(last.color, first.color, first.x > from ? (o - from) / (first.x - from) : 1.0f);
        }
        if (_wrap) {
            return last.color; // a hard first key: the loop holds the last colour until it cuts in
        }
        return first.color; // not looping: the first colour from the start
    }
    if (next == _colorKeys.size()) {
        // after the last key
        if (loop) {
            const float to = first.x + 1.0f;
            return mix(last.color, first.color, to > last.x ? (o - last.x) / (to - last.x) : 0.0f);
        }
        return last.color;
    }
    const vcColorKey& a = _colorKeys[next - 1];
    const vcColorKey& b = _colorKeys[next];
    if (!b.blend || b.x <= a.x) {
        return a.color;
    }
    return mix(a.color, b.color, (o - a.x) / (b.x - a.x));
}

const vcSortablePoint* ValueCurve::FindPoint(float x) const
{
    for (const auto& it : _values) {
        if (it == x) {
            return &it;
        }
    }
    return nullptr;
}

void ValueCurve::SetPointShape(float x, vcSortablePoint::Shape shape)
{
    for (auto it = _values.begin(); it != _values.end(); ++it) {
        if (!(*it == x)) {
            continue;
        }
        if (shape == vcSortablePoint::Shape::Custom && it->shape != vcSortablePoint::Shape::Custom) {
            // start from the eased shape: flat handles a third of the way to each neighbour
            auto prev = it == _values.begin() ? _values.end() : std::prev(it);
            auto next = std::next(it);
            it->inDx = prev != _values.end() ? -(it->x - prev->x) / 3.0f : 0.0f;
            it->outDx = next != _values.end() ? (next->x - it->x) / 3.0f : 0.0f;
            it->inDy = 0.0f;
            it->outDy = 0.0f;
        }
        it->shape = shape;
        break;
    }
    if (_type == "Custom" && _parameter1 == 1.0f) {
        _baseCustomValues = _values;
    }
}

void ValueCurve::CopyPointShape(float x, const vcSortablePoint& from)
{
    for (auto& it : _values) {
        if (it == x) {
            it.CopyShape(from);
            break;
        }
    }
    if (_type == "Custom" && _parameter1 == 1.0f) {
        _baseCustomValues = _values;
    }
}

void ValueCurve::SetPointHandles(float x, float inDx, float inDy, float outDx, float outDy)
{
    for (auto& it : _values) {
        if (it == x) {
            it.inDx = std::min(0.0f, inDx);
            it.inDy = inDy;
            it.outDx = std::max(0.0f, outDx);
            it.outDy = outDy;
            break;
        }
    }
    if (_type == "Custom" && _parameter1 == 1.0f) {
        _baseCustomValues = _values;
    }
}

void ValueCurve::SegmentControls(const vcSortablePoint& a, const vcSortablePoint& b, float& c1x, float& c1y, float& c2x, float& c2y)
{
    const float len = b.x - a.x;
    switch (a.shape) {
    case vcSortablePoint::Shape::Ease:
        c1x = a.x + len / 3.0f;
        c1y = a.y;
        break;
    case vcSortablePoint::Shape::Custom:
        // kept inside the segment so the curve stays a function of x
        c1x = a.x + std::clamp(a.outDx, 0.0f, len);
        c1y = a.y + a.outDy;
        break;
    default:
        c1x = a.x;
        c1y = a.y;
        break;
    }
    switch (b.shape) {
    case vcSortablePoint::Shape::Ease:
        c2x = b.x - len / 3.0f;
        c2y = b.y;
        break;
    case vcSortablePoint::Shape::Custom:
        c2x = b.x + std::clamp(b.inDx, -len, 0.0f);
        c2y = b.y + b.inDy;
        break;
    default:
        c2x = b.x;
        c2y = b.y;
        break;
    }
}

float ValueCurve::SegmentValue(const vcSortablePoint& a, const vcSortablePoint& b, float x)
{
    const float len = b.x - a.x;
    if (len <= 0.0f) {
        return b.y;
    }
    if (a.IsHard() && b.IsHard()) {
        return a.y + (b.y - a.y) * (x - a.x) / len;
    }
    float c1x, c1y, c2x, c2y;
    SegmentControls(a, b, c1x, c1y, c2x, c2y);
    auto bez = [](float p0, float p1, float p2, float p3, float t) {
        const float u = 1.0f - t;
        return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t * p3;
    };
    // the t where the curve reaches x (x(t) rises from a.x to b.x)
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 30; ++i) {
        const float mid = (lo + hi) / 2.0f;
        if (bez(a.x, c1x, c2x, b.x, mid) < x) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return bez(a.y, c1y, c2y, b.y, (lo + hi) / 2.0f);
}

float ValueCurve::GetPointAt(float x) const
{
    for (const auto& it : _values) {
        if (it.x == x)
            return it.y;
    }

    return 0.0;
}

void ValueCurve::SetPointAt(float x, float y)
{
    for (auto& it : _values) {
        if (it.x == x) {
            it.y = y;
            break;
        }
    }

    if (_type == "Custom" && _parameter1 == 1.0f) {
        _baseCustomValues = _values;
    }
}

void ValueCurve::ScaleAndOffsetValues(float scale, int offset)
{
    if (offset == 0 && abs(scale - 1.0) < 0.0001) {
        return;
    }
    
    auto ScaleVal = [&](float val, float range) -> float {
        const float valScaled = val * range;
        float newVal = (valScaled * (scale * _divisor)) + (offset * _divisor);
        newVal = std::min(newVal, _max);
        newVal = std::max(newVal, _min);
        return newVal / range;
    };

    std::vector<int> parametersToScale;
    if (_type == "Custom") {
        // custom values are 0-1, so we need to scale them
        float range = _max - _min;
        if (std::abs(range) <= std::numeric_limits<float>::epsilon()) {
            assert(false); // shouldn't be zero
            return;
        }
        for (auto& it : _values) {
            it.y = ScaleVal(it.y, range);
            it.inDy *= scale * _divisor;
            it.outDy *= scale * _divisor;
        }
        if (_parameter1 == 1.0f) {
            _baseCustomValues = _values;
        } else {
            ReconstructBaseCustomValues();
        }
    } else if (_type == "Flat") {
        parametersToScale.push_back(1);
    } else if (_type == "Ramp" || _type == "Ramp Up/Down Hold" || _type == "Saw Tooth" || _type == "Square" || _type == "Random" || _type == "Music" || _type == "Inverted Music" || 
               _type == "Music Trigger Fade" || _type == "Timing Track Toggle" || _type == "Timing Track Fade Fixed" || _type == "Timing Track Fade Proportional") {
        parametersToScale.push_back(1);
        parametersToScale.push_back(2);
    } else if (_type == "Ramp Up/Down") {
        parametersToScale.push_back(1);
        parametersToScale.push_back(2);
        parametersToScale.push_back(3);
    } else if (_type == "Parabolic Down" || _type == "Parabolic Up" || _type == "Logarithmic Up" || _type == "Logarithmic Down" || _type == "Exponential Up" || _type == "Exponential Down") {
        parametersToScale.push_back(2);
    } else if (_type == "Sine" || _type == "Abs Sine" || _type == "Decaying Sine") {
        parametersToScale.push_back(4);
    } else {
        parametersToScale.push_back(1);
        parametersToScale.push_back(2);
        parametersToScale.push_back(3);
        parametersToScale.push_back(4);
    }
    //_parameter1 are min to max
    for (int param : parametersToScale) {
        switch (param) {
        case 1:
            _parameter1 = ScaleVal(_parameter1, 1.0F);
            break;
        case 2:
            _parameter2 = ScaleVal(_parameter2, 1.0F);
            break;
        case 3:
            _parameter3 = ScaleVal(_parameter3, 1.0F);
            break;
        case 4:
            _parameter4 = ScaleVal(_parameter4, 1.0F);
            break;
        }
    }    
}

void ValueCurve::ReconstructBaseCustomValues() {
    _baseCustomValues.clear();
    int totalPoints = _values.size();
    float cycles = _parameter1;
    if (cycles < 1.0f) cycles = 1.0f;
    if (cycles > 10.0f) cycles = 10.0f;
    int c = std::round(cycles);
    if (c < 1) c = 1;
    if (c > 10) c = 10;

    if (c == 1 || totalPoints == 0) {
        _baseCustomValues = _values;
    } else {
        int N = totalPoints / c;
        if (N < 2) N = 2; // custom curves always have at least 2 points (0.0 and 1.0)
        
        int count = 0;
        for (const auto& pt : _values) {
            if (count >= N) break;
            float newX = pt.x * c;
            if (newX > 1.0f) newX = 1.0f;
            vcSortablePoint np(newX, pt.y, pt.wrapped);
            np.CopyShape(pt);
            np.inDx *= c;
            np.outDx *= c;
            _baseCustomValues.push_back(np);
            count++;
        }
        _baseCustomValues.sort();
    }
}
