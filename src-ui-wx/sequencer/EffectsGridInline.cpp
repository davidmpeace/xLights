/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

// Inline editing: an effect's value curve (and its colour keys) edited right on the effect in the grid.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <vector>

#include <wx/wx.h>

#include "EffectsGrid.h"
#include "MainSequencer.h"
#include "color/xlColourData.h"
#include "effects/RenderableEffect.h"
#include "render/CommandExecutor.h"
#include "shared/utils/wxUtilities.h"
#include "xLightsMain.h"

namespace {
constexpr int POINT_HIT = 7;   // px from a point that picks it
constexpr int KEY_HIT = 6;     // px from a colour key that picks it
constexpr float POINT_SIZE = 3.5f;
constexpr int STRIP_HEIGHT = 18;

const xlColor POINT_COLOUR(255, 40, 40);
const xlColor SELECTED_COLOUR(120, 200, 255);
const xlColor HANDLE_COLOUR(255, 160, 40);
const xlColor SEPARATOR_COLOUR(128, 128, 128);
constexpr int HANDLE_HIT = 6;

void Quad(xlVertexColorAccumulator* bg, float xa, float ya, float xb, float yb, const xlColor& c) {
    bg->AddVertex(xa, ya, c);
    bg->AddVertex(xa, yb, c);
    bg->AddVertex(xb, yb, c);
    bg->AddVertex(xb, yb, c);
    bg->AddVertex(xb, ya, c);
    bg->AddVertex(xa, ya, c);
}

// A line from a to b, `half` wide either side.
void Segment(xlVertexColorAccumulator* bg, float ax, float ay, float bx, float by, float half, const xlColor& c) {
    const float len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    if (len <= 0.0f) {
        return;
    }
    const float nx = -(by - ay) / len * half;
    const float ny = (bx - ax) / len * half;
    bg->AddVertex(ax + nx, ay + ny, c);
    bg->AddVertex(ax - nx, ay - ny, c);
    bg->AddVertex(bx - nx, by - ny, c);
    bg->AddVertex(bx - nx, by - ny, c);
    bg->AddVertex(bx + nx, by + ny, c);
    bg->AddVertex(ax + nx, ay + ny, c);
}

void Disc(xlVertexColorAccumulator* bg, float cx, float cy, float r, const xlColor& c) {
    constexpr int SEGMENTS = 16;
    for (int i = 0; i < SEGMENTS; ++i) {
        const float a0 = (float)i / SEGMENTS * 2.0f * 3.14159265f;
        const float a1 = (float)(i + 1) / SEGMENTS * 2.0f * 3.14159265f;
        bg->AddVertex(cx, cy, c);
        bg->AddVertex(cx + std::cos(a0) * r, cy + std::sin(a0) * r, c);
        bg->AddVertex(cx + std::cos(a1) * r, cy + std::sin(a1) * r, c);
    }
}

std::vector<vcSortablePoint> PointsOf(const ValueCurve& vc) {
    const auto pts = vc.GetPoints();
    return std::vector<vcSortablePoint>(pts.begin(), pts.end());
}

void SetPoints(ValueCurve& vc, const std::vector<vcSortablePoint>& pts) {
    vc.SetBaseCustomPoints(std::list<vcSortablePoint>(pts.begin(), pts.end()));
}

// A pointing hand with a plus: Ctrl/Cmd-click here adds a point or colour key.
const wxCursor& AddCursor() {
    static const char* const xpm[] = {
        "32 32 3 1",
        "  c None",
        ". c #000000",
        "X c #FFFFFF",
        "     ..                         ",
        "    .XX.                        ",
        "    .XX.                        ",
        "    .XX.                        ",
        "    .XX.                        ",
        "    .XX...                      ",
        "    .XX.XX...                   ",
        "    .XX.XX.XX..                 ",
        " .. .XX.XX.XX.X.                ",
        ".XX..XX.XX.XX.X.                ",
        ".XXX.XXXXXXXXXX.                ",
        " .XX.XXXXXXXXXX.                ",
        "  .XXXXXXXXXXXX.                ",
        "  .XXXXXXXXXXXX.                ",
        "   .XXXXXXXXXXX.                ",
        "   .XXXXXXXXXX.      XXXX       ",
        "    .XXXXXXXXX.      X..X       ",
        "    .XXXXXXXXX.      X..X       ",
        "     .XXXXXXX.    XXXX..XXXX    ",
        "     .XXXXXXX.    X........X    ",
        "     .........    X........X    ",
        "                  XXXX..XXXX    ",
        "                     X..X       ",
        "                     X..X       ",
        "                     XXXX       ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
        "                                ",
    };
    static const wxCursor cursor = [] {
        wxImage img(xpm);
        img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 5);
        img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 1);
        return wxCursor(img);
    }();
    return cursor;
}

// The id a value curve setting's curve carries: E_VALUECURVE_x -> ID_VALUECURVE_x.
std::string CurveId(const std::string& setting) {
    return "ID_" + (setting.size() > 2 ? setting.substr(2) : setting);
}

// Edits a point's position (% of the curve) and value.
class InlinePointDialog : public wxDialog {
public:
    InlinePointDialog(wxWindow* parent, float x, float value, bool fixedX, float maxX, float minValue, float maxValue) :
        wxDialog(parent, wxID_ANY, "Edit Point"), _maxX(maxX), _min(minValue), _max(maxValue) {
        auto* grid = new wxFlexGridSizer(2, 5, 8);
        grid->Add(new wxStaticText(this, wxID_ANY, "Position (%)"), 0, wxALIGN_CENTER_VERTICAL);
        _x = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.1f", x * 100.0f));
        _x->Enable(!fixedX);
        grid->Add(_x, 1, wxEXPAND);
        grid->Add(new wxStaticText(this, wxID_ANY, wxString::Format("Value (%g-%g)", _min, _max)), 0, wxALIGN_CENTER_VERTICAL);
        _value = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.1f", value));
        grid->Add(_value, 1, wxEXPAND);
        auto* top = new wxBoxSizer(wxVERTICAL);
        top->Add(grid, 1, wxEXPAND | wxALL, 10);
        top->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
        SetSizerAndFit(top);
        _value->SetFocus();
        _value->SelectAll();
    }
    float GetX() const { return std::clamp((float)std::strtod(_x->GetValue().c_str(), nullptr) / 100.0f, 0.0f, _maxX); }
    float GetValue() const { return std::clamp((float)std::strtod(_value->GetValue().c_str(), nullptr), _min, _max); }

private:
    wxTextCtrl* _x;
    wxTextCtrl* _value;
    float _maxX, _min, _max;
};
}

bool EffectsGrid::IsInlineEditing(const Effect* e) const {
    if (e == nullptr || e->GetParentEffectLayer() == nullptr || e->GetParentEffectLayer()->GetRowHeight() < INLINE_EDIT_ROW_HEIGHT) {
        return false;
    }
    RenderableEffect* re = xlights->GetEffectManager()[e->GetEffectIndex()];
    return re != nullptr && re->SupportsInlineEdit();
}

bool EffectsGrid::GetInlineGeom(Effect* e, int row, InlineEditGeom& g) const {
    if (!IsInlineEditing(e)) {
        return false;
    }
    RenderableEffect* re = xlights->GetEffectManager()[e->GetEffectIndex()];
    if (re == nullptr || !re->SupportsInlineEdit()) {
        return false;
    }
    g.effect = e;
    g.re = re;
    g.row = row;
    g.hasColor = re->InlineEditHasColor();
    g.hasCurve = re->InlineEditHasCurve();
    g.baseColor = re->GetInlineEditBaseColor(e);

    const int rowTop = mSequenceElements->GetRowTop(row);
    const int rowBottom = mSequenceElements->GetRowBottom(row);
    g.barTop = rowTop + 2;
    g.barBottom = g.barTop + std::max(8, DEFAULT_ROW_HEADING_HEIGHT - 4);
    g.stripBottom = rowBottom - 3;
    g.stripTop = g.hasColor ? g.stripBottom - STRIP_HEIGHT : g.stripBottom;
    g.bodyTop = g.barBottom + 2;
    g.bodyBottom = g.hasColor ? g.stripTop - 3 : g.stripBottom;
    if (g.bodyBottom - g.bodyTop < 16) {
        return false; // too short a row to edit in
    }

    EFFECT_SCREEN_MODE mode;
    int x1, x2, x3, x4;
    mTimeline->GetPositionsFromTimeRange(e->GetStartTimeMS(), e->GetEndTimeMS(), mode, x1, x2, x3, x4);
    g.x0 = (float)x3;
    g.x1 = (float)x4;

    g.xmax = 0.0f;
    if (re->GetInlineEditCurveSpan(e, g.start, g.scale) && g.scale > 0.0f && g.start < 1.0f) {
        g.xmax = std::min(1.0f, (1.0f - g.start) / g.scale);
    }
    return true;
}

bool EffectsGrid::InlineHit(int x, int y, InlineEditGeom& g) const {
    if (mSequenceElements == nullptr || mTimeline == nullptr) {
        return false;
    }
    const int row = GetRow(y);
    if (row < mSequenceElements->GetNumberOfTimingRows() || row >= (int)mSequenceElements->GetVisibleRowInformationSize()) {
        return false;
    }
    EffectLayer* layer = mSequenceElements->GetEffectLayer(mSequenceElements->GetVisibleRowInformation(row));
    if (layer == nullptr) {
        return false;
    }
    for (int i = 0; i < layer->GetEffectCount(); ++i) {
        Effect* e = layer->GetEffect(i);
        if (!GetInlineGeom(e, row, g)) {
            continue;
        }
        if (x >= g.x0 && x <= g.x1 && y >= g.bodyTop && y <= g.stripBottom) {
            return true;
        }
    }
    return false;
}

bool EffectsGrid::LoadInlineCurve(const InlineEditGeom& g, ValueCurve& vc) const {
    const std::string setting = g.re->GetInlineEditCurveSetting();
    float min = 0.0f, max = 100.0f;
    g.re->GetInlineEditCurveRange(min, max);
    const std::string s = g.effect->GetSettings().Get(setting, "");
    vc = ValueCurve(CurveId(setting), min, max, "Custom");
    vc.SetDivisor(1);
    if (!s.empty()) {
        vc.Deserialise(s);
        vc.SetLimits(min, max);
    }
    if (vc.IsActive() && vc.GetType() != "Custom") {
        vc.SetType("Custom"); // an inline curve is a custom one: from the points its old type drew
    } else if (!vc.IsActive()) {
        // not on yet: flat at the value the effect has without it (it goes on with the first edit)
        const float flat = max > min ? std::clamp((g.re->GetInlineEditFlatValue(g.effect) - min) / (max - min), 0.0f, 1.0f) : 0.0f;
        vc = ValueCurve(CurveId(setting), min, max, "Custom");
        vc.SetDivisor(1);
        vc.SetValueAt(0.0f, flat);
        vc.SetValueAt(1.0f, flat);
        vc.SetActive(true);
    }
    return true;
}

void EffectsGrid::ApplyInlineCurve(Effect* e, RenderableEffect* re, ValueCurve vc, bool undo) {
    const std::string setting = re->GetInlineEditCurveSetting();
    const std::string value = vc.Serialise();
    auto& exec = mSequenceElements->GetCommandExecutor();
    if (!undo) {
        exec.SetEffectSetting(e, setting, value, false);
        return;
    }
    // one undo step from where the edit started
    exec.SetEffectSetting(e, setting, mInlineBefore, false);
    if (value == mInlineBefore) {
        return;
    }
    mSequenceElements->get_undo_mgr().CreateUndoStep();
    exec.SetEffectSetting(e, setting, value, true);
    RenderInlineEffect(e);
    if (e == mSelectedEffect) {
        RaiseSelectedEffectChanged(e, false, true); // the settings panel shows the new curve
    }
}

void EffectsGrid::ClearInlineSelection() {
    mInlineEffect = nullptr;
    mInlineSelX = -1.0f;
    mInlineKey = -1;
    mInlineColor = false;
}

bool EffectsGrid::DeleteInlineSelection() {
    if (mInlineEffect == nullptr || mSequenceElements == nullptr || !mSequenceElements->IsValidEffect(mInlineEffect) ||
        !IsInlineEditing(mInlineEffect)) {
        ClearInlineSelection();
        return false;
    }
    const int row = GetEffectRow(mInlineEffect) - mSequenceElements->GetFirstVisibleModelRow();
    InlineEditGeom g;
    if (!GetInlineGeom(mInlineEffect, row, g)) {
        return false;
    }
    ValueCurve vc;
    LoadInlineCurve(g, vc);
    mInlineBefore = vc.Serialise();
    LoadInlineCurve(g, vc);
    if (mInlineColor) {
        auto keys = vc.GetColorKeys();
        if (mInlineKey < 0 || mInlineKey >= (int)keys.size()) {
            return false;
        }
        keys.erase(keys.begin() + mInlineKey);
        vc.SetColorKeys(std::move(keys));
    } else {
        auto pts = PointsOf(vc);
        auto it = std::find_if(pts.begin(), pts.end(), [&](const vcSortablePoint& p) { return p.x == mInlineSelX; });
        // the ends stay
        if (it == pts.end() || it == pts.begin() || it == pts.end() - 1) {
            return it != pts.end();
        }
        pts.erase(it);
        SetPoints(vc, pts);
    }
    ClearInlineSelection();
    ApplyInlineCurve(g.effect, g.re, vc, true);
    Draw();
    return true;
}

int EffectsGrid::InlinePointAt(const InlineEditGeom& g, const ValueCurve& vc, float x, float y) const {
    if (!g.hasCurve) {
        return -1;
    }
    const auto pts = PointsOf(vc);
    int best = -1;
    float bestD = POINT_HIT + 1;
    for (int i = 0; i < (int)pts.size(); ++i) {
        if (pts[i].x > g.xmax + 0.0001f) {
            continue;
        }
        const float dx = g.CurveToX(pts[i].x) - x;
        const float dy = g.LevelToY(pts[i].y) - y;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d <= POINT_HIT && d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

int EffectsGrid::InlineKeyAt(const InlineEditGeom& g, const ValueCurve& vc, float x) const {
    const auto& keys = vc.GetColorKeys();
    int best = -1;
    float bestD = KEY_HIT + 1;
    for (int i = 0; i < (int)keys.size(); ++i) {
        if (keys[i].x > g.xmax + 0.0001f) {
            continue;
        }
        const float d = std::abs(g.CurveToX(keys[i].x) - x);
        if (d <= KEY_HIT && d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

int EffectsGrid::InlineHandleAt(const InlineEditGeom& g, const ValueCurve& vc, float x, float y) const {
    if (!g.hasCurve || mInlineEffect != g.effect || mInlineColor) {
        return 0;
    }
    const auto pts = PointsOf(vc);
    for (int i = 0; i < (int)pts.size(); ++i) {
        const vcSortablePoint& p = pts[i];
        if (p.x != mInlineSelX || p.shape != vcSortablePoint::Shape::Custom) {
            continue;
        }
        auto isNear = [&](float hx, float hy) {
            return std::abs(g.CurveToX(hx) - x) <= HANDLE_HIT && std::abs(g.LevelToY(hy) - y) <= HANDLE_HIT;
        };
        if (i < (int)pts.size() - 1 && isNear(p.x + p.outDx, p.y + p.outDy)) {
            return 1;
        }
        if (i > 0 && isNear(p.x + p.inDx, p.y + p.inDy)) {
            return -1;
        }
        return 0;
    }
    return 0;
}

bool EffectsGrid::InlineRightClick(wxMouseEvent& event) {
    InlineEditGeom g;
    if (!InlineHit(event.GetX(), event.GetY(), g)) {
        return false;
    }
    ValueCurve vc;
    LoadInlineCurve(g, vc);
    const float x = (float)event.GetX();
    const float y = (float)event.GetY();
    const bool inStrip = g.hasColor && y >= g.stripTop;
    const int index = inStrip ? InlineKeyAt(g, vc, x) : InlinePointAt(g, vc, x, y);
    if (index < 0) {
        return false; // anywhere else: the effect's own menu
    }
    SetFocus();
    if (mSelectedEffect != g.effect || g.effect->GetSelected() == EFFECT_NOT_SELECTED) {
        mSequenceElements->UnSelectAllEffects();
        SelectEffect(g.effect);
    }
    ClearInlineSelection();
    mInlineEffect = g.effect;
    mInlineColor = inStrip;
    // starts the edit from the curve as it is now
    auto fresh = [this, g](ValueCurve& c) {
        LoadInlineCurve(g, c);
        mInlineBefore = c.Serialise();
        LoadInlineCurve(g, c);
    };

    wxMenu menu;
    if (inStrip) {
        mInlineKey = index;
        const int idBlend = wxNewId(), idHard = wxNewId();
        menu.AppendRadioItem(idBlend, "Blend");
        menu.AppendRadioItem(idHard, "Hard");
        menu.Check(vc.GetColorKeys()[index].blend ? idBlend : idHard, true);
        menu.Bind(wxEVT_MENU, [this, g, index, idBlend, fresh](wxCommandEvent& e) {
            ValueCurve c;
            fresh(c);
            auto keys = c.GetColorKeys();
            if (index >= (int)keys.size()) {
                return;
            }
            keys[index].blend = e.GetId() == idBlend;
            c.SetColorKeys(std::move(keys));
            ApplyInlineCurve(g.effect, g.re, c, true);
            Draw();
        });
    } else {
        const vcSortablePoint p = PointsOf(vc)[index];
        mInlineSelX = p.x;
        const int idHard = wxNewId(), idEase = wxNewId(), idCustom = wxNewId();
        menu.AppendRadioItem(idHard, "Hard Point");
        menu.AppendRadioItem(idEase, "Ease");
        menu.AppendRadioItem(idCustom, "Custom");
        menu.Check(p.shape == vcSortablePoint::Shape::Ease ? idEase : p.shape == vcSortablePoint::Shape::Custom ? idCustom : idHard, true);
        menu.Bind(wxEVT_MENU, [this, g, px = p.x, idEase, idCustom, fresh](wxCommandEvent& e) {
            const vcSortablePoint::Shape shape = e.GetId() == idEase ? vcSortablePoint::Shape::Ease
                                               : e.GetId() == idCustom ? vcSortablePoint::Shape::Custom
                                                                       : vcSortablePoint::Shape::Hard;
            ValueCurve c;
            fresh(c);
            c.SetPointShape(px, shape);
            SetPoints(c, PointsOf(c));
            ApplyInlineCurve(g.effect, g.re, c, true);
            Draw();
        });
    }
    Draw();
    PopupMenu(&menu);
    return true;
}

bool EffectsGrid::UpdateInlineCursor(int x, int y, bool adding) {
    InlineEditGeom g;
    if (!InlineHit(x, y, g)) {
        return false;
    }
    ValueCurve vc;
    LoadInlineCurve(g, vc);
    const float cx = g.XToCurve((float)x);
    const bool inSpan = g.xmax > 0.0f && cx >= 0.0f && cx <= g.xmax;
    const bool inStrip = g.hasColor && y >= g.stripTop;
    const bool onOne = inStrip ? InlineKeyAt(g, vc, (float)x) >= 0
                               : InlinePointAt(g, vc, (float)x, (float)y) >= 0 || InlineHandleAt(g, vc, (float)x, (float)y) != 0;
    static const wxCursor s_hand(wxCURSOR_HAND);
    static const wxCursor s_arrow(wxCURSOR_ARROW);
    if (onOne) {
        SetCursor(s_hand);
    } else if (adding && inSpan && (inStrip || g.hasCurve)) {
        SetCursor(AddCursor());
    } else {
        SetCursor(s_arrow);
    }
    return true;
}

void EffectsGrid::OnInlineCursorTimer(wxTimerEvent&) {
    if (mInlineDragging) {
        return;
    }
    const wxMouseState ms = wxGetMouseState();
    const wxPoint p = ScreenToClient(wxPoint(ms.GetX(), ms.GetY()));
    if (!UpdateInlineCursor(p.x, p.y, ms.ControlDown() || ms.RawControlDown())) {
        mInlineCursorTimer.Stop(); // off the curve: the next mouse move sets the cursor
    }
}

void EffectsGrid::RenderInlineEffect(Effect* e) {
    Element* el = e->GetParentEffectLayer() != nullptr ? e->GetParentEffectLayer()->GetParentElement() : nullptr;
    if (el != nullptr) {
        sendRenderEvent(el->GetModelName(), e->GetStartTimeMS(), e->GetEndTimeMS(), false);
    }
}

bool EffectsGrid::InlineMouseDown(wxMouseEvent& event) {
    mInlineClick = false;
    InlineEditGeom g;
    if (!InlineHit(event.GetX(), event.GetY(), g)) {
        if (mInlineEffect != nullptr) {
            ClearInlineSelection();
            Draw();
        }
        return false;
    }
    mInlineClick = true;
    SetFocus();
    if (!(g.hasColor && event.GetY() >= g.stripTop)) {
        // a Bezier handle of the selected (Custom) point
        ValueCurve vc;
        LoadInlineCurve(g, vc);
        if (const int handle = InlineHandleAt(g, vc, (float)event.GetX(), (float)event.GetY()); handle != 0) {
            mInlineBefore = vc.Serialise();
            LoadInlineCurve(g, vc);
            mInlineVc = vc;
            mInlineHandle = handle;
            mInlineRow = g.row;
            mInlineDragging = true;
            if (!HasCapture()) {
                CaptureMouse();
            }
            Draw();
            return true;
        }
    }
    ClearInlineSelection();
    if (mSelectedEffect != g.effect || g.effect->GetSelected() == EFFECT_NOT_SELECTED) {
        // a click on the effect selects it, as anywhere else on it would
        if (!event.ShiftDown()) {
            mSequenceElements->UnSelectAllEffects();
        }
        SelectEffect(g.effect);
    }
    // Ctrl/Cmd-click in a free spot adds a point or colour key
    const bool adding = event.ControlDown() || event.RawControlDown();
    ValueCurve vc;
    LoadInlineCurve(g, vc);
    mInlineBefore = vc.Serialise();
    LoadInlineCurve(g, vc);
    const float x = (float)event.GetX();
    const float y = (float)event.GetY();
    const float cx = g.XToCurve(x);
    const bool inSpan = g.xmax > 0.0f && cx >= 0.0f && cx <= g.xmax;

    if (g.hasColor && y >= g.stripTop) {
        auto keys = vc.GetColorKeys();
        const int best = InlineKeyAt(g, vc, x);
        if (best >= 0) {
            mInlineEffect = g.effect;
            mInlineColor = true;
            mInlineKey = best;
            mInlineIndex = best;
        } else if (adding && inSpan) {
            // a new key, in the colour the curve has there; Cancel in the picker takes it away again
            mInlineEffect = g.effect;
            mInlineColor = true;
            vcColorKey k;
            k.x = cx;
            k.color = keys.empty() ? g.baseColor : vc.GetColorAt(cx);
            keys.push_back(k);
            std::stable_sort(keys.begin(), keys.end(), [](const vcColorKey& a, const vcColorKey& b) { return a.x < b.x; });
            mInlineKey = (int)(std::find_if(keys.begin(), keys.end(), [&](const vcColorKey& a) { return a.x == cx; }) - keys.begin());
            vc.SetColorKeys(keys);
            ApplyInlineCurve(g.effect, g.re, vc, false);
            Draw();
            auto apply = [&](const wxColour& c) {
                keys[mInlineKey].color = wxColourToXlColor(c);
                vc.SetColorKeys(keys);
                ApplyInlineCurve(g.effect, g.re, vc, false);
                RenderInlineEffect(g.effect);
                Draw();
            };
            auto const& [res, colour] = xlColourData::INSTANCE.ShowColorDialog(this, xlColorToWxColour(k.color), apply);
            if (res == wxID_OK) {
                apply(colour);
                ApplyInlineCurve(g.effect, g.re, vc, true);
            } else {
                ApplyInlineCurve(g.effect, g.re, ValueCurve(mInlineBefore), false);
                RenderInlineEffect(g.effect);
                ClearInlineSelection();
            }
            Draw();
            return true;
        } else {
            Draw();
            return true;
        }
    } else if (!g.hasCurve) {
        Draw(); // nothing to edit above the colour keys: the click only selected the effect
        return true;
    } else {
        auto pts = PointsOf(vc);
        int best = InlinePointAt(g, vc, x, y);
        if (best < 0) {
            if (!adding || !inSpan) {
                Draw();
                return true;
            }
            // a new point where the mouse is
            const float px = vcSortablePoint::Normalise(cx);
            if (px <= 0.0f || px >= 1.0f || std::any_of(pts.begin(), pts.end(), [&](const vcSortablePoint& p) { return p.x == px; })) {
                Draw();
                return true;
            }
            pts.emplace_back(px, g.YToLevel(y), false);
            std::sort(pts.begin(), pts.end(), [](const vcSortablePoint& a, const vcSortablePoint& b) { return a.x < b.x; });
            best = (int)(std::find_if(pts.begin(), pts.end(), [&](const vcSortablePoint& p) { return p.x == px; }) - pts.begin());
            SetPoints(vc, pts);
            ApplyInlineCurve(g.effect, g.re, vc, false);
        }
        mInlineEffect = g.effect;
        mInlineColor = false;
        mInlineSelX = pts[best].x;
        mInlineIndex = best;
    }

    mInlineVc = vc;
    mInlineRow = g.row;
    mInlineDragging = true;
    if (!HasCapture()) {
        CaptureMouse();
    }
    Draw();
    return true;
}

bool EffectsGrid::InlineMouseMove(wxMouseEvent& event) {
    if (!mInlineDragging) {
        // over an inline curve: no move/resize handles here
        if (UpdateInlineCursor(event.GetX(), event.GetY(), event.ControlDown() || event.RawControlDown())) {
            mResizingMode = 0; // EFFECT_RESIZE_NO
            if (!mInlineCursorTimerBound) {
                mInlineCursorTimer.SetOwner(this);
                Bind(wxEVT_TIMER, &EffectsGrid::OnInlineCursorTimer, this, mInlineCursorTimer.GetId());
                mInlineCursorTimerBound = true;
            }
            if (!mInlineCursorTimer.IsRunning()) {
                mInlineCursorTimer.Start(100);
            }
            return true;
        }
        return false;
    }
    InlineEditGeom g;
    if (mInlineEffect == nullptr || !mSequenceElements->IsValidEffect(mInlineEffect) || !GetInlineGeom(mInlineEffect, mInlineRow, g)) {
        mInlineDragging = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
        ClearInlineSelection();
        return true;
    }
    const float x = (float)event.GetX();
    const float cx = std::clamp(g.XToCurve(x), 0.0f, g.xmax);

    if (mInlineHandle != 0) {
        // the selected point's handle: on its own side of the point and within the segment; with Shift/Ctrl/Cmd on a
        // point with both, the other one goes exactly opposite (and neither passes a neighbour)
        auto pts = PointsOf(mInlineVc);
        auto it = std::find_if(pts.begin(), pts.end(), [&](const vcSortablePoint& p) { return p.x == mInlineSelX; });
        if (it == pts.end()) {
            return true;
        }
        const bool hasPrev = it != pts.begin();
        const bool hasNext = it + 1 != pts.end();
        const float before = hasPrev ? it->x - (it - 1)->x : 0.0f;
        const float after = hasNext ? (it + 1)->x - it->x : 0.0f;
        const bool mirror = (event.ShiftDown() || event.ControlDown() || event.RawControlDown()) && hasPrev && hasNext;
        float reach = mInlineHandle < 0 ? before : after;
        if (mirror) {
            reach = std::min(before, after);
        }
        const float hx = g.XToCurve(x);
        const float hy = ((g.bodyBottom - 2) - (float)event.GetY()) / std::max(1, g.bodyBottom - g.bodyTop - 4);
        const float dx = mInlineHandle < 0 ? std::clamp(hx - it->x, -reach, 0.0f) : std::clamp(hx - it->x, 0.0f, reach);
        const float dy = hy - it->y;
        float inDx = it->inDx, inDy = it->inDy, outDx = it->outDx, outDy = it->outDy;
        if (mInlineHandle < 0 && hasPrev) {
            inDx = dx;
            inDy = dy;
            if (mirror) {
                outDx = -dx;
                outDy = -dy;
            }
        } else if (mInlineHandle > 0 && hasNext) {
            outDx = dx;
            outDy = dy;
            if (mirror) {
                inDx = -dx;
                inDy = -dy;
            }
        }
        mInlineVc.SetPointHandles(it->x, inDx, inDy, outDx, outDy);
        SetPoints(mInlineVc, PointsOf(mInlineVc));
        ApplyInlineCurve(g.effect, g.re, mInlineVc, false);
        Draw();
        return true;
    }

    if (mInlineColor) {
        auto keys = mInlineVc.GetColorKeys();
        if (mInlineIndex < 0 || mInlineIndex >= (int)keys.size()) {
            return true;
        }
        keys[mInlineIndex].x = cx;
        // keep track of the dragged key as it passes others
        const vcColorKey moved = keys[mInlineIndex];
        keys.erase(keys.begin() + mInlineIndex);
        auto at = std::upper_bound(keys.begin(), keys.end(), moved, [](const vcColorKey& a, const vcColorKey& b) { return a.x < b.x; });
        mInlineIndex = (int)(at - keys.begin());
        keys.insert(at, moved);
        mInlineKey = mInlineIndex;
        mInlineVc.SetColorKeys(keys);
    } else {
        auto pts = PointsOf(mInlineVc);
        if (mInlineIndex < 0 || mInlineIndex >= (int)pts.size()) {
            return true;
        }
        vcSortablePoint p = pts[mInlineIndex];
        float level = g.YToLevel((float)event.GetY());
        if (event.ShiftDown() || event.ControlDown() || event.CmdDown()) {
            level = std::round(level * 20.0f) / 20.0f; // whole 5s
        }
        p.y = level;
        const bool end = mInlineIndex == 0 || mInlineIndex == (int)pts.size() - 1;
        if (!end) {
            // past its neighbours it just moves on by; the ends stay at 0 and 1
            const float nx = std::clamp(vcSortablePoint::Normalise(cx), vcSortablePoint::perPoint(), 1.0f - vcSortablePoint::perPoint());
            const bool taken = std::any_of(pts.begin(), pts.end(), [&](const vcSortablePoint& o) { return &o != &pts[mInlineIndex] && o.x == nx; });
            if (!taken) {
                p.x = nx;
            }
        }
        pts.erase(pts.begin() + mInlineIndex);
        auto at = std::upper_bound(pts.begin(), pts.end(), p, [](const vcSortablePoint& a, const vcSortablePoint& b) { return a.x < b.x; });
        if (!end) {
            // never ahead of the first point or after the last
            at = std::clamp(at, pts.begin() + 1, pts.end() - 1);
        }
        mInlineIndex = (int)(at - pts.begin());
        pts.insert(at, p);
        mInlineSelX = p.x;
        SetPoints(mInlineVc, pts);
    }
    ApplyInlineCurve(g.effect, g.re, mInlineVc, false);
    Draw();
    return true;
}

bool EffectsGrid::InlineMouseUp(wxMouseEvent& event) {
    const bool click = mInlineClick;
    mInlineClick = false;
    if (!mInlineDragging) {
        return click; // a click that picked nothing (or opened the colour picker): not the grid's either
    }
    mInlineDragging = false;
    mInlineHandle = 0;
    if (HasCapture()) {
        ReleaseMouse();
    }
    InlineEditGeom g;
    if (mInlineEffect != nullptr && mSequenceElements->IsValidEffect(mInlineEffect) && GetInlineGeom(mInlineEffect, mInlineRow, g)) {
        ApplyInlineCurve(g.effect, g.re, mInlineVc, true);
    }
    Draw();
    return true;
}

bool EffectsGrid::InlineDoubleClick(wxMouseEvent& event) {
    InlineEditGeom g;
    if (!InlineHit(event.GetX(), event.GetY(), g)) {
        return false;
    }
    mInlineClick = true;
    mInlineDragging = false;
    mInlineHandle = 0;
    if (HasCapture()) {
        ReleaseMouse();
    }
    if (mInlineEffect != g.effect) {
        return true; // the first click picked nothing here
    }
    ValueCurve vc;
    LoadInlineCurve(g, vc);
    mInlineBefore = vc.Serialise();
    LoadInlineCurve(g, vc);
    if (mInlineColor) {
        auto keys = vc.GetColorKeys();
        if (mInlineKey < 0 || mInlineKey >= (int)keys.size()) {
            return true;
        }
        auto apply = [&](const wxColour& c) {
            keys[mInlineKey].color = wxColourToXlColor(c);
            vc.SetColorKeys(keys);
            ApplyInlineCurve(g.effect, g.re, vc, false);
            RenderInlineEffect(g.effect);
            Draw();
        };
        auto const& [res, colour] = xlColourData::INSTANCE.ShowColorDialog(this, xlColorToWxColour(keys[mInlineKey].color), apply);
        if (res == wxID_OK) {
            apply(colour);
            ApplyInlineCurve(g.effect, g.re, vc, true);
        } else {
            ApplyInlineCurve(g.effect, g.re, ValueCurve(mInlineBefore), false);
            RenderInlineEffect(g.effect);
        }
        Draw();
        return true;
    }
    auto pts = PointsOf(vc);
    auto it = std::find_if(pts.begin(), pts.end(), [&](const vcSortablePoint& p) { return p.x == mInlineSelX; });
    if (it == pts.end()) {
        return true;
    }
    float min = 0.0f, max = 100.0f;
    g.re->GetInlineEditCurveRange(min, max);
    const bool end = it == pts.begin() || it == pts.end() - 1;
    InlinePointDialog dlg(this, it->x, min + it->y * (max - min), end, std::min(1.0f, g.xmax), min, max);
    OptimiseDialogPosition(&dlg);
    if (dlg.ShowModal() != wxID_OK) {
        return true;
    }
    vcSortablePoint p = *it;
    p.y = max > min ? (dlg.GetValue() - min) / (max - min) : 0.0f;
    if (!end) {
        const float nx = std::clamp(vcSortablePoint::Normalise(dlg.GetX()), vcSortablePoint::perPoint(), 1.0f - vcSortablePoint::perPoint());
        if (std::none_of(pts.begin(), pts.end(), [&](const vcSortablePoint& o) { return &o != &*it && o.x == nx; })) {
            p.x = nx;
        }
    }
    pts.erase(it);
    pts.push_back(p);
    std::sort(pts.begin(), pts.end(), [](const vcSortablePoint& a, const vcSortablePoint& b) { return a.x < b.x; });
    SetPoints(vc, pts);
    mInlineSelX = p.x;
    ApplyInlineCurve(g.effect, g.re, vc, true);
    Draw();
    return true;
}

void EffectsGrid::DrawInlineEdit(const InlineEditGeom& g, xlVertexColorAccumulator* bg) {
    ValueCurve vc;
    if (mInlineDragging && g.effect == mInlineEffect) {
        vc = mInlineVc; // as dragged, with any point that lines up with its neighbours
    } else {
        LoadInlineCurve(g, vc);
    }
    const bool selectedHere = mInlineEffect == g.effect;
    const float xa = g.CurveToX(0.0f);
    const float xb = g.CurveToX(g.xmax);

    // thin grey lines: the top of the curve's area (below the bar that moves and resizes the effect), and above the
    // colour strip
    Quad(bg, g.x0, g.bodyTop - 1, g.x1, g.bodyTop, SEPARATOR_COLOUR);
    if (g.hasColor) {
        Quad(bg, g.x0, g.stripTop - 2, g.x1, g.stripTop - 1, SEPARATOR_COLOUR);
    }

    if (g.hasCurve && g.xmax > 0.0f && xb > xa) {
        // the curve: a black line with a white edge, as far as it can be edited
        const int steps = std::clamp((int)(xb - xa), 2, 600);
        std::vector<std::pair<float, float>> line;
        line.reserve(steps + 1);
        ValueCurve sample = vc;
        for (int i = 0; i <= steps; ++i) {
            const float cx = g.xmax * i / steps;
            line.emplace_back(g.CurveToX(cx), g.LevelToY(sample.GetValueAt(cx, 0, 1)));
        }
        for (float half : { 2.0f, 1.0f }) {
            const xlColor c = half > 1.5f ? xlWHITE : xlBLACK;
            for (size_t i = 1; i < line.size(); ++i) {
                Segment(bg, line[i - 1].first, line[i - 1].second, line[i].first, line[i].second, half, c);
            }
        }
        // its points
        for (const auto& p : PointsOf(vc)) {
            if (p.x > g.xmax + 0.0001f) {
                continue;
            }
            const float px = g.CurveToX(p.x);
            const float py = g.LevelToY(p.y);
            const bool sel = selectedHere && !mInlineColor && p.x == mInlineSelX;
            Quad(bg, px - POINT_SIZE - 1, py - POINT_SIZE - 1, px + POINT_SIZE + 1, py + POINT_SIZE + 1, xlBLACK);
            Quad(bg, px - POINT_SIZE, py - POINT_SIZE, px + POINT_SIZE, py + POINT_SIZE, sel ? SELECTED_COLOUR : POINT_COLOUR);
        }
        // the selected point's Bezier handles, when it is a Custom one
        const auto pts = PointsOf(vc);
        for (int i = 0; selectedHere && !mInlineColor && i < (int)pts.size(); ++i) {
            const vcSortablePoint& p = pts[i];
            if (p.x != mInlineSelX || p.shape != vcSortablePoint::Shape::Custom) {
                continue;
            }
            const float px = g.CurveToX(p.x);
            const float py = g.LevelToY(p.y);
            for (int side : { -1, 1 }) {
                if ((side < 0 && i == 0) || (side > 0 && i == (int)pts.size() - 1)) {
                    continue;
                }
                const float hx = g.CurveToX(p.x + (side < 0 ? p.inDx : p.outDx));
                const float hy = g.LevelToY(p.y + (side < 0 ? p.inDy : p.outDy));
                Segment(bg, px, py, hx, hy, 0.75f, HANDLE_COLOUR);
                Disc(bg, hx, hy, 5.0f, mInlineDragging && mInlineHandle == side ? xlWHITE : xlBLACK);
                Disc(bg, hx, hy, 4.0f, HANDLE_COLOUR);
            }
        }
    }

    if (!g.hasColor || g.xmax <= 0.0f || xb <= xa) {
        return;
    }
    // the colour keys: the curve's colours across the part that can be edited, with a marker per key
    Quad(bg, xa - 1, g.stripTop - 1, xb + 1, g.stripBottom + 1, xlBLACK);
    const int steps = std::clamp((int)(xb - xa) / 2, 1, 400);
    for (int i = 0; i < steps; ++i) {
        const float c0 = g.xmax * i / steps;
        const float c1 = g.xmax * (i + 1) / steps;
        Quad(bg, g.CurveToX(c0), g.stripTop, g.CurveToX(c1), g.stripBottom, vc.HasColorKeys() ? vc.GetColorAt((c0 + c1) / 2.0f) : g.baseColor);
    }
    const float cy = (g.stripTop + g.stripBottom) / 2.0f;
    const float r = STRIP_HEIGHT / 2.0f - 1.0f;
    const auto& keys = vc.GetColorKeys();
    for (int i = 0; i < (int)keys.size(); ++i) {
        if (keys[i].x > g.xmax + 0.0001f) {
            continue;
        }
        const float kx = g.CurveToX(keys[i].x);
        const bool sel = selectedHere && mInlineColor && i == mInlineKey;
        const xlColor ring = sel ? SELECTED_COLOUR : xlWHITE;
        // circle: blends into it; square: cuts to it
        if (keys[i].blend) {
            Disc(bg, kx, cy, r + 1.5f, xlBLACK);
            Disc(bg, kx, cy, r, ring);
            Disc(bg, kx, cy, r - 2.0f, keys[i].color);
        } else {
            Quad(bg, kx - r - 1.5f, cy - r - 1.5f, kx + r + 1.5f, cy + r + 1.5f, xlBLACK);
            Quad(bg, kx - r, cy - r, kx + r, cy + r, ring);
            Quad(bg, kx - r + 2.0f, cy - r + 2.0f, kx + r - 2.0f, cy + r - 2.0f, keys[i].color);
        }
    }
}
