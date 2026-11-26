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

#include "RenderableEffect.h"

// Drives a fountain from one value curve: its value is the pumps' level (0-100%) and its colour keyframes the lights'
// colour, over the effect. All plays it on every pump at once; Chase on one pump after another, overlapping by
// Overlap % (0: one at a time). On a fountain (or its submodels) pumps take the level and lights the colour; on any other
// model every pixel takes the colour.
class FountainsEffect : public RenderableEffect
{
public:
    FountainsEffect(int id);
    virtual ~FountainsEffect();
    virtual bool CanBeRandom() override { return false; }
    virtual void Render(Effect* effect, const SettingsMap& settings, RenderBuffer& buffer) override;
    virtual FrameParallelism GetFrameParallelism(const SettingsMap& settings) const override { return FrameParallelism::Pure; }
    virtual bool CanRenderPartialTimeInterval() const override { return true; }
    virtual bool BackgroundNeedsPalette() const override { return false; } // the curve's colour keys are its colours
    // Inline editing shows the first pump's first turn of the curve (as the grid's bar does).
    virtual bool SupportsInlineEdit() const override { return true; }
    virtual std::string GetInlineEditCurveSetting() const override { return "E_VALUECURVE_Fountains_Curves"; }
    virtual bool InlineEditHasColor() const override { return true; }
    virtual bool GetInlineEditCurveSpan(const Effect* e, float& start, float& scale) const override;
    // The grid shows the first pump's colour over time (as tall as its level with ramps on).
    virtual int DrawEffectBackground(const Effect* e, int x1, int y1, int x2, int y2, xlVertexColorAccumulator& backgrounds,
                                     xlColor* colorMask, bool ramps) override;
};
