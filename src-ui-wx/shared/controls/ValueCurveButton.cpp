/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "ValueCurveButton.h"
#include "shared/utils/ValueCurveRendering.h"

#include <wx/artprov.h>
#include <wx/dcmemory.h>
#include <wx/dnd.h>

wxDEFINE_EVENT(EVT_VC_CHANGED, wxCommandEvent);

class ValueCurveButton;

class VCTextDropTarget : public wxTextDropTarget
{
public:
    VCTextDropTarget(ValueCurveButton* owner, wxString type) { _owner = owner; _type = type; };

    virtual bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override;
    virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult def) override;

    ValueCurveButton* _owner;
    wxString _type;
};

wxDragResult VCTextDropTarget::OnDragOver(wxCoord x, wxCoord y, wxDragResult def)
{
    if (_owner->IsEnabled())
    {
        return wxDragCopy;
    }
    return wxDragNone;
}

bool VCTextDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& data)
{
    if (data == "") return false;

    if (!_owner->IsEnabled()) return false;

    if (data == "VALUECURVE_CLEAR")
    {
        _owner->SetActive(false);
    }
    else
    {
        if (!data.Contains("ID_VALUECURVE")) return false;

        ValueCurve vc(data);
        ValueCurve* pvc = _owner->GetValue();
        vc.SetId(pvc->GetId());
        vc.SetLimits(pvc->GetMin(), pvc->GetMax());
        vc.SetDivisor(pvc->GetDivisor());
        vc.FixChangedScale(pvc->GetMin(), pvc->GetMax(), 1);

        _owner->GetValue()->Deserialise(vc.Serialise());
        _owner->UpdateBitmap();
        _owner->NotifyChange();
    }

    return true;
}

ValueCurveButton::ValueCurveButton(wxWindow *parent,
    wxWindowID id,
    const wxBitmapBundle& bitmap,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& validator,
    const wxString& name) : wxBitmapButton(parent, id, bitmap, pos, size, style, validator, name)
{
    _vc = new ValueCurve(name.ToStdString());

    VCTextDropTarget* vcdt = new VCTextDropTarget(this, "ValueCurve");
    SetDropTarget(vcdt);
}

ValueCurveButton::~ValueCurveButton()
{
    if (_vc != nullptr)
    {
        delete _vc;
    }
}

void ValueCurveButton::ResetToDefault()
{
    if (_alwaysActive) {
        _vc->SetActive(false);
        SetActive(true);
    } else {
        SetActive(false);
    }
}

// A fresh flat custom curve at the top: no other points, no colours, no wrap.
void ValueCurveButton::MakeDefaultCurve()
{
    _vc->SetType("Flat"); // drops any custom points left from before
    _vc->SetType("Custom");
    _vc->SetValueAt(0.0f, 0.0f);
    _vc->SetValueAt(1.0f, 0.0f);
    _vc->SetColorKeys({});
    _vc->SetWrap(false);
    _vc->SetViewMax(0.0f);
}

void ValueCurveButton::SetAlwaysActive(bool b)
{
    _alwaysActive = b;
    if (b && !_vc->IsActive()) {
        // called while the panel is being built: set the curve up without telling anyone (nothing is listening yet)
        MakeDefaultCurve();
        _vc->SetActive(true);
        UpdateBitmap();
    }
}

void ValueCurveButton::SetActive(bool active)
{
    if (_alwaysActive && !active) {
        active = true;
    }
    if (_alwaysActive && active && !_vc->IsActive()) {
        MakeDefaultCurve();
    }
    bool isActive = _vc->IsActive();
    if (isActive != active) {
        _vc->SetActive(active);
        UpdateState();
    } else {
        UpdateBitmap();
    }
}

void ValueCurveButton::ToggleActive()
{
    if (_alwaysActive) {
        SetActive(true);
        return;
    }
    _vc->ToggleActive();
    UpdateState();
}

void ValueCurveButton::UpdateBitmap() {
    if (GetValue()->IsActive()) {
        RenderNewBitmap();
    } else {
        static wxBitmapBundle nsBitmap = wxArtProvider::GetBitmapBundle("xlART_valuecurve_notselected", wxART_BUTTON);
        SetBitmap(nsBitmap);
    }
}

void ValueCurveButton::UpdateState()
{
    UpdateBitmap();
    NotifyChange();
}

void ValueCurveButton::RenderNewBitmap() {
    SetBitmap(GetValueCurveImage(*_vc, 24, 24, GetContentScaleFactor()));
}

void ValueCurveButton::SetValue(const wxString& value)
{
    _vc->Deserialise(value.ToStdString());
    if (_alwaysActive && !_vc->IsActive()) {
        SetActive(true); // a setting saved without its curve still gets one
        return;
    }
    UpdateState();
}

void ValueCurveButton::NotifyChange()
{
    wxCommandEvent eventVCChange(EVT_VC_CHANGED);
    eventVCChange.SetEventObject(this);
    GetParent()->ProcessWindowEvent(eventVCChange);
}

void ValueCurveButton::SetLimits(float min, float max)
{
    _vc->SetLimits(min, max);
    UpdateState();
}

ValueCurve* ValueCurveButton::GetValue()
{
    return _vc;
}
