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

#include <wx/bmpbuttn.h>
#include "render/ValueCurve.h"

wxDECLARE_EVENT(EVT_VC_CHANGED, wxCommandEvent);

class ValueCurveButton :
    public wxBitmapButton
{
    ValueCurve* _vc;
    bool _supportsColor = false;
    bool _alwaysActive = false;
    void MakeDefaultCurve();
    void RenderNewBitmap();

public:
    ValueCurveButton(wxWindow *parent,
        wxWindowID id,
        const wxBitmapBundle& bitmap,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxBU_AUTODRAW,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxButtonNameStr);
    ~ValueCurveButton();
    virtual void SetValue(const wxString& value);
    void SetLimits(float min, float max);
    ValueCurve* GetValue();
    void ToggleActive();
    void SetActive(bool active);
    void UpdateState();
    void UpdateBitmap();
    void NotifyChange();
    // The curve's editor also offers colour keyframes (see ValueCurveDialog::EnableColor).
    void SetSupportsColor(bool b) { _supportsColor = b; }
    bool SupportsColor() const { return _supportsColor; }
    // A curve that is never switched off (the setting is the curve, with no slider): clicking only opens the
    // editor, and it starts as a flat custom curve at the max.
    void SetAlwaysActive(bool b);
    bool IsAlwaysActive() const { return _alwaysActive; }
    // Off, or for an always-active curve a fresh default one.
    void ResetToDefault();
};
