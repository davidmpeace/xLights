/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "../../include/On.xpm"

#include "OnEffect.h"
#include "../render/Effect.h"
#include "../render/RenderBuffer.h"
#include "UtilClasses.h"
#include "UtilFunctions.h"
#include "Parallel.h"
#include "../render/ValueCurve.h"
#include <log.h>

#include <algorithm>
#include <cmath>

static const std::string TEXTCTRL_Eff_On_Start("TEXTCTRL_Eff_On_Start");
static const std::string TEXTCTRL_Eff_On_End("TEXTCTRL_Eff_On_End");
static const std::string CHECKBOX_On_Shimmer("CHECKBOX_On_Shimmer");
static const std::string TEXTCTRL_On_Cycles("TEXTCTRL_On_Cycles");

// Fallback defaults (used until OnMetadataLoaded replaces them with On.json values).
int OnEffect::sStartDefault = 100;
int OnEffect::sEndDefault = 100;
bool OnEffect::sShimmerDefault = false;
float OnEffect::sCyclesDefault = 1.0f;
int OnEffect::sTransparencyDefault = 0;
int OnEffect::sTransparencyMin = 0;
int OnEffect::sTransparencyMax = 100;

OnEffect::OnEffect(int i) : RenderableEffect(i, "On", On, On, On, On, On)
{
    //ctor
}

OnEffect::~OnEffect()
{
    //dtor
}

void OnEffect::OnMetadataLoaded()
{
    sStartDefault = GetIntDefault("Eff_On_Start", sStartDefault);
    sEndDefault = GetIntDefault("Eff_On_End", sEndDefault);
    sShimmerDefault = GetBoolDefault("On_Shimmer", sShimmerDefault);
    sCyclesDefault = GetFloatDefault("On_Cycles", sCyclesDefault);
    sTransparencyDefault = GetIntDefault("On_Transparency", sTransparencyDefault);
    sTransparencyMin = (int)GetMinFromMetadata("On_Transparency", sTransparencyMin);
    sTransparencyMax = (int)GetMaxFromMetadata("On_Transparency", sTransparencyMax);
}

namespace {
// The effect's colour keys (COLORS_SETTING; "On_Colors" in Render's settings), when it has any.
bool LoadOnColors(const std::string& s, ValueCurve& vc) {
    if (s.empty()) {
        return false;
    }
    vc.SetDivisor(1);
    vc.SetLimits(0, 100);
    vc.Deserialise(s);
    return vc.IsActive() && vc.HasColorKeys();
}

// How far through its cycle `position` (0-1 through the effect) is; the end of a cycle is its end, not a restart.
float CyclePosition(float position, float cycles) {
    if (cycles <= 0.0f) {
        return 0.0f;
    }
    const float t = position * cycles;
    const float u = t - std::floor(t);
    return (u == 0.0f && t > 0.0f) ? 1.0f : u;
}

// The colour at `adjust` (0-1 through a cycle), dimmed by the intensity ramp.
xlColor OnColorAt(ValueCurve& vc, float adjust, int start, int end) {
    xlColor c = vc.GetColorAt(adjust);
    if (start != 100 || end != 100) {
        HSVValue hsv = c.asHSV();
        hsv.value = hsv.value * (start + (end - start) * adjust) / 100.0;
        c = hsv;
    }
    return c;
}
}

bool OnEffect::GetInlineEditCurveSpan(const Effect* e, float& start, float& scale) const
{
    // the first cycle (as much of it as there is)
    const float cycles = e->GetSettings().GetFloat("E_TEXTCTRL_On_Cycles", sCyclesDefault);
    if (cycles <= 0.0f) {
        return false;
    }
    start = 0.0f;
    scale = 1.0f / cycles;
    return true;
}

xlColor OnEffect::GetInlineEditBaseColor(const Effect* e) const
{
    // until it has colour keys, the effect shows its first palette colour
    return e->GetPalette().empty() ? xlBLACK : e->GetPalette()[0];
}

void GetOnEffectColors(const Effect *e, xlColor &start, xlColor &end) {
    int starti = e->GetSettings().GetInt("E_TEXTCTRL_Eff_On_Start", 100);
    int endi = e->GetSettings().GetInt("E_TEXTCTRL_Eff_On_End", 100);
    xlColor newcolor = e->GetPalette()[0];
    if (starti == 100 && endi == 100) {
        start = end = newcolor;
    } else {
        HSVValue hsv = newcolor.asHSV();
        hsv.value = (hsv.value * starti) / 100;
        start = hsv;
        hsv = newcolor.asHSV();
        hsv.value = (hsv.value * endi) / 100;
        end = hsv;
    }
}

int OnEffect::DrawEffectBackground(const Effect *e, int x1, int y1, int x2, int y2,
                                   xlVertexColorAccumulator &bg, xlColor* colorMask, bool ramp) 
{
    ValueCurve colors;
    if (LoadOnColors(e->GetSettings().Get(COLORS_SETTING, ""), colors)) {
        // the colour over time, cycle by cycle, as bright as the intensity ramp has it (and as high, with ramps on)
        const int starti = e->GetSettings().GetInt("E_TEXTCTRL_Eff_On_Start", sStartDefault);
        const int endi = e->GetSettings().GetInt("E_TEXTCTRL_Eff_On_End", sEndDefault);
        const float cycles = e->GetSettings().GetFloat("E_TEXTCTRL_On_Cycles", sCyclesDefault);
        const int width = x2 - x1;
        if (width <= 0) {
            return 2;
        }
        const int columns = std::clamp(width, 1, 400);
        for (int i = 0; i < columns; ++i) {
            const float xa = x1 + (float)width * i / columns;
            const float xb = x1 + (float)width * (i + 1) / columns;
            const float adjust = CyclePosition((i + 0.5f) / columns, cycles);
            xlColor c = OnColorAt(colors, adjust, starti, endi);
            c.ApplyMask(colorMask);
            const float level = (starti + (endi - starti) * adjust) / 100.0f;
            const float top = ramp ? y2 - level * (y2 - y1) : (float)y1;
            bg.AddVertex(xa, top, c);
            bg.AddVertex(xa, y2, c);
            bg.AddVertex(xb, y2, c);
            bg.AddVertex(xb, y2, c);
            bg.AddVertex(xb, top, c);
            bg.AddVertex(xa, top, c);
        }
        return 2;
    }
    if (ramp) {
        bool shimmer = e->GetSettings().GetInt("E_CHECKBOX_On_Shimmer", 0) > 0;
        int starti = e->GetSettings().GetInt("E_TEXTCTRL_Eff_On_Start", 100);
        int endi = e->GetSettings().GetInt("E_TEXTCTRL_Eff_On_End", 100);
        xlColor color = e->GetPalette()[0];
        color.ApplyMask(colorMask);
        int height = y2 - y1;

        float starty = y2 - starti * height / 100.0f;
        float endy = y2 - endi * height / 100.0f;
        float m = float(endy - starty) / float(x2 - x1);

        bg.AddVertex(x1, starty, color);
        bg.AddVertex(x1, y2, color);
        bg.AddVertex(x2, y2, color);
        bg.AddVertex(x2, endy, color);
        bg.AddVertex(x2, y2, color);
        bg.AddVertex(x1, starty, color);

        if (shimmer)
        {
            const int gap = 3;
            for (int x = x1 + (gap*3); x < x2; x += (gap*3)) {
                float newY = m * (x - x1) + starty;
                float newY2 = m * (x + (gap) - x1) + starty;
                bg.AddVertex(x, y2, xlBLACK);
                bg.AddVertex(x, newY, xlBLACK);
                bg.AddVertex(x + (gap), newY2, xlBLACK);

                bg.AddVertex(x, y2, xlBLACK);
                bg.AddVertex(x + (gap), y2, xlBLACK);
                bg.AddVertex(x + (gap), newY2, xlBLACK);
            }
        }

        return 2;
    } else {
        if (e->HasBackgroundDisplayList()) {
            e->GetBackgroundDisplayList().addToAccumulator(x1, y1, x2 - x1, y2 - y1, bg);
            return e->GetBackgroundDisplayList().iconSize;
        }
        xlColor start;
        xlColor end;
        GetOnEffectColors(e, start, end);

        start.ApplyMask(colorMask);
        end.ApplyMask(colorMask);

        bg.AddVertex(x1, y1, start);
        bg.AddVertex(x1, y2, start);
        bg.AddVertex(x2, y2, end);

        bg.AddVertex(x2, y2, end);
        bg.AddVertex(x2, y1, end);
        bg.AddVertex(x1, y1, start);
        return 2;
    }
}

void OnEffect::Render(Effect *eff, const SettingsMap &SettingsMap, RenderBuffer &buffer) {
    // Defaults were cached from On.json in OnMetadataLoaded() — Render must
    // never touch the JSON.
    int start = SettingsMap.GetInt(TEXTCTRL_Eff_On_Start, sStartDefault);
    int end = SettingsMap.GetInt(TEXTCTRL_Eff_On_End, sEndDefault);
    bool shimmer = SettingsMap.GetInt(CHECKBOX_On_Shimmer, sShimmerDefault ? 1 : 0) > 0;
    float cycles = SettingsMap.GetFloat(TEXTCTRL_On_Cycles, sCyclesDefault);
    
    int cidx = 0;
    if (shimmer) {
        int tot = buffer.curPeriod - buffer.curEffStartPer;
        if (tot % 2) {
            if (buffer.palette.Size() <= 1) {
                return;
            }
            cidx = 1;
        }
    }

    // colour keys stand in for the first palette colour
    ValueCurve colors;
    const bool keyed = cidx == 0 && LoadOnColors(SettingsMap.Get("On_Colors", xlEMPTY_STRING), colors);

    bool spatialcolour = !keyed && buffer.palette.IsSpatial(cidx);
    bool gradientcolour = !keyed && buffer.palette.IsGradient(cidx);
    double adjust = buffer.GetEffectTimeIntervalPosition(cycles);

    xlColor color;
    if (keyed) {
        color = OnColorAt(colors, (float)adjust, start, end);
    } else if (start == 100 && end == 100) {
        buffer.palette.GetColor(cidx, color);
    } else {
        HSVValue hsv;
        buffer.palette.GetHSV(cidx,hsv);
        double d = adjust;
        d = start + (end - start) * d;
        d = d / 100.0;
        hsv.value = hsv.value * d;
        color = hsv;
    }

    int transparency = GetValueCurveInt("On_Transparency", sTransparencyDefault, SettingsMap, adjust,
                                        sTransparencyMin, sTransparencyMax,
                                        buffer.GetStartTimeMS(), buffer.GetEndTimeMS());
    if (transparency) {
        transparency *= 255;
        transparency /= 100;
        color.alpha = 255 - transparency;
    }

    //Every Node set to selected color
    if (spatialcolour || buffer.dmx_buffer) {

        // doing x in parallel for large buffers
        parallel_for(0, buffer.BufferWi, [&buffer, cidx, start, end, adjust, transparency, color](int x) {
            auto col = color;
            for (int y = 0; y < buffer.BufferHt; ++y) {
                if (buffer.palette.IsRadial(cidx)) {
                    float zeroedx = (x - (buffer.BufferWi / 2));
                    float zeroedy = (y - (buffer.BufferHt / 2));
                    float round = 0;
                    if (zeroedx != 0 || zeroedy != 0) {
                        float signx = zeroedx > 0.0 ? 1.0 : (zeroedx < 0.0 ? -1.0 : 0.0);
                        float signy = zeroedy > 0.0 ? 1.0 : (zeroedy < 0.0 ? -1.0 : 0.0);
                        float signxy = zeroedx * zeroedy > 0.0 ? 1.0 : (zeroedx * zeroedy < 0.0 ? -1.0 : 0.0);
                        float signsignysigny = signy * signy > 0.0 ? 1.0 : (signy * signy < 0.0 ? -1.0 : 0.0);
                        round = (PI - PI / 2.0 * (1.0 + signx) * (1.0 - signsignysigny) - PI / 4.0 * (2.0 + signx) * signy - signxy * std::asin((std::abs(zeroedx) - std::abs(zeroedy)) / std::sqrt(2.0 * zeroedx * zeroedx + 2.0 * zeroedy * zeroedy))) / (2.0 * PI);
                    }
                    buffer.palette.GetSpatialColor(cidx, buffer.BufferWi / 2, buffer.BufferHt / 2, x, y, round, std::max(buffer.BufferWi, buffer.BufferHt) / 2, col);
                } else {
                    buffer.palette.GetSpatialColor(cidx, (float)x / (float)buffer.BufferWi, (float)y / (float)buffer.BufferHt, col);
                }
                if (start == 100 && end == 100) {
                } else {
                    HSVValue hsv = col.asHSV();
                    double d = adjust;
                    d = start + (end - start) * d;
                    d = d / 100.0;
                    hsv.value = hsv.value * d;
                    col = hsv;
                }
                if (transparency) {
                    col.alpha = 255 - transparency;
                }
                buffer.SetPixel(x, y, col);
            }
        });
    } else {
        buffer.Fill(color);
    }

    if (shimmer || cycles != 1.0 || gradientcolour || spatialcolour || keyed) {
        if (eff->IsBackgroundDisplayListEnabled() && buffer.perModelIndex == 0) {
            std::lock_guard<std::recursive_mutex> lock(eff->GetBackgroundDisplayList().lock);
            eff->GetBackgroundDisplayList().resize((buffer.curEffEndPer - buffer.curEffStartPer + 1) * 6);
            buffer.CopyPixelsToDisplayListX(eff, 0, 0, 0);
        }
    } else if (buffer.needToInit) {
        if (eff->IsBackgroundDisplayListEnabled() && buffer.perModelIndex == 0) {
            std::lock_guard<std::recursive_mutex> lock(eff->GetBackgroundDisplayList().lock);
            eff->GetBackgroundDisplayList().resize(6);
            if (start == 100 && end == 100) {
                buffer.palette.GetColor(0, color);
                buffer.SetDisplayListHRect(eff, 0, 0.0, 0.0, 1.0, 1.0, color, color);
            } else {
                HSVValue hsv;
                buffer.palette.GetHSV(cidx, hsv);
                hsv.value = hsv.value * start / 100.0;
                color = hsv;

                buffer.palette.GetHSV(cidx, hsv);
                hsv.value = hsv.value * end / 100.0;
                buffer.SetDisplayListHRect(eff, 0, 0.0, 0.0, 1.0, 1.0, color, xlColor(hsv));
            }
        }
        buffer.needToInit = false;
    }
}
