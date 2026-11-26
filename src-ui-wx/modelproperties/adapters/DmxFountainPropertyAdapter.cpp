/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>

#include "DmxFountainPropertyAdapter.h"
#include "../helpers/DmxAbilityPropertyHelpers.h"
#include "models/DMX/DmxFountain.h"
#include "models/OutputModelManager.h"

DmxFountainPropertyAdapter::DmxFountainPropertyAdapter(Model& model)
    : DmxPropertyAdapter(model), _fountain(static_cast<DmxFountain&>(model)) {}

void DmxFountainPropertyAdapter::AddTypeProperties(wxPropertyGridInterface* grid, OutputManager* outputManager) {
    auto p = grid->Append(new wxUIntProperty("# Channels", "DmxChannelCount", _fountain.GetDmxChannelCount()));
    p->SetAttribute("Min", 1);
    p->SetAttribute("Max", 512);
    p->SetEditor("SpinCtrl");

    grid->Append(new wxPropertyCategory("Fountain", "FountainProperties"));
    p = grid->Append(new wxUIntProperty("Pumps", "FountainPumps", _fountain.GetPumpCount()));
    p->SetAttribute("Min", 1);
    p->SetAttribute("Max", DmxFountain::MAX_PUMPS);
    p->SetEditor("SpinCtrl");

    wxArrayString arrangements;
    for (const auto& label : DmxFountain::ArrangementLabels()) {
        arrangements.push_back(label);
    }
    p = grid->Append(new wxEnumProperty("Arrangement", "FountainArrangement", arrangements, wxArrayInt(), (int)_fountain.GetArrangement()));
    p->SetHelpString("Ring with Center: the last pump is in the center. Square: the pumps are spaced evenly around the edge.");
    p = grid->Append(new wxBoolProperty("Number Clockwise", "FountainClockwise", _fountain.IsClockwise()));
    p->SetAttribute("UseCheckbox", true);
    p->SetHelpString("Number the pumps clockwise (as seen from above) around a ring or square. Pump 1 is at the front.");
    p = grid->Append(new wxUIntProperty("Nozzle Spread %", "FountainNozzleSpread", _fountain.GetNozzleSpread()));
    p->SetAttribute("Min", 0);
    p->SetAttribute("Max", 100);
    p->SetEditor("SpinCtrl");
    p->SetHelpString("How far out the nozzles sit: the ring's diameter, the line's length or the square's or grid's side, as a percentage of the model's width (the pool).");
    p = grid->Append(new wxFloatProperty("Max Water Height (ft)", "FountainMaxHeight", _fountain.GetMaxHeightFt()));
    p->SetAttribute("Min", 0.5);
    p->SetAttribute("Max", 500);
    p->SetAttribute("Precision", 1);
    p->SetAttribute("Step", 0.5);
    p->SetEditor("SpinCtrlDouble");
    p->SetHelpString("How high the water goes at 100%. It sets the physics (how long the water flies); the model's height is how tall the tallest jet is drawn.");
    if (_fountain.HasCenterPump()) {
        p = grid->Append(new wxFloatProperty("Center Max Water Height (ft)", "FountainCenterMaxHeight", _fountain.GetCenterMaxHeightFt()));
        p->SetAttribute("Min", 0.5);
        p->SetAttribute("Max", 500);
        p->SetAttribute("Precision", 1);
        p->SetAttribute("Step", 0.5);
        p->SetEditor("SpinCtrlDouble");
        p->SetHelpString("How high the center pump's water goes at 100%. It follows Max Water Height until you change it; set it back to the same value to follow again.");
    }

    p = grid->Append(new wxUIntProperty("Pump Start Channel", "FountainPumpChannel", _fountain.GetPumpChannel()));
    p->SetAttribute("Min", 1);
    p->SetAttribute("Max", 512);
    p->SetEditor("SpinCtrl");
    p->SetHelpString("Channel of pump 1; each pump is one channel (0-255 = off to the max water height).");
    p = grid->Append(new wxBoolProperty("RGB Lights", "FountainLights", _fountain.HasLights()));
    p->SetAttribute("UseCheckbox", true);
    p = grid->Append(new wxUIntProperty("Light Start Channel", "FountainLightChannel", _fountain.GetLightChannel()));
    p->SetAttribute("Min", 1);
    p->SetAttribute("Max", 512);
    p->SetEditor("SpinCtrl");
    p->SetHelpString("Red channel of pump 1's light; each light is Red, Green, Blue.");
    p->Enable(_fountain.HasLights());

    grid->Append(new wxPropertyCategory("Fountain Preview", "FountainPreviewProperties"));
    p = grid->Append(new wxBoolProperty("Spray", "FountainSpray", _fountain.IsSprayEnabled()));
    p->SetAttribute("UseCheckbox", true);
    p->SetHelpString("Draw the spray where falling water lands on rising water.");
    p = grid->Append(new wxBoolProperty("Show Pool", "FountainPool", _fountain.IsPoolShown()));
    p->SetAttribute("UseCheckbox", true);
    p = grid->Append(new wxUIntProperty("Droplets per Pump", "FountainDroplets", _fountain.GetDropletsPerPump()));
    p->SetAttribute("Min", 50);
    p->SetAttribute("Max", 5000);
    p->SetEditor("SpinCtrl");

    if (_fountain.HasPresetAbility()) {
        DmxAbilityPropertyHelpers::AddPresetProperties(grid, *_fountain.GetPresetAbility(), _fountain.GetDmxChannelCount());
    }

    grid->Append(new wxPropertyCategory("Common Properties", "CommonProperties"));
}

void DmxFountainPropertyAdapter::DisableUnusedProperties(wxPropertyGridInterface* grid) {
    for (const char* name : { "ModelPixelSize", "ModelPixelStyle", "ModelStringType", "ModelStringColor", "ModelFaces", "ModelDimmingCurves" }) {
        wxPGProperty* p = grid->GetPropertyByName(name);
        if (p != nullptr) {
            p->Enable(false);
        }
    }
}

int DmxFountainPropertyAdapter::OnPropertyGridChange(wxPropertyGridInterface* grid, wxPropertyGridEvent& event) {
    const wxString name = event.GetPropertyName();
    // changes that move channels: rename them and grow the model to fit
    auto channelsChanged = [&](const char* why) {
        _fountain.UpdateNodeNames();
        _fountain.IncrementChangeCount();
        _fountain.UpdateChannelCount(std::max(_fountain.GetDmxChannelCount(), _fountain.GetRequiredChannelCount()), true);
        _fountain.AddASAPWork(OutputModelManager::WORK_RGBEFFECTS_CHANGE |
                              OutputModelManager::WORK_RELOAD_PROPERTYGRID |
                              OutputModelManager::WORK_REDRAW_LAYOUTPREVIEW, why);
    };
    auto redraw = [&](const char* why) {
        _fountain.IncrementChangeCount();
        _fountain.AddASAPWork(OutputModelManager::WORK_RELOAD_MODEL_CHANGE |
                              OutputModelManager::WORK_RGBEFFECTS_CHANGE |
                              OutputModelManager::WORK_REDRAW_LAYOUTPREVIEW, why);
    };

    if (name == "FountainPumps") {
        const bool sequential = _fountain.GetLightChannel() == _fountain.GetPumpChannel() + _fountain.GetPumpCount();
        _fountain.SetPumpCount((int)event.GetPropertyValue().GetLong());
        if (sequential) {
            // the lights followed the pumps: keep them right after
            _fountain.SetLightChannel(_fountain.GetPumpChannel() + _fountain.GetPumpCount());
        }
        channelsChanged("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainPumps");
        return 0;
    }
    if (name == "FountainPumpChannel") {
        const bool sequential = _fountain.GetLightChannel() == _fountain.GetPumpChannel() + _fountain.GetPumpCount();
        _fountain.SetPumpChannel((int)event.GetPropertyValue().GetLong());
        if (sequential) {
            _fountain.SetLightChannel(_fountain.GetPumpChannel() + _fountain.GetPumpCount());
        }
        channelsChanged("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainPumpChannel");
        return 0;
    }
    if (name == "FountainLights") {
        _fountain.SetHasLights(event.GetPropertyValue().GetBool());
        channelsChanged("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainLights");
        return 0;
    }
    if (name == "FountainLightChannel") {
        _fountain.SetLightChannel((int)event.GetPropertyValue().GetLong());
        channelsChanged("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainLightChannel");
        return 0;
    }
    if (name == "FountainArrangement") {
        // the submodels and the center pump's height row come and go with the center pump
        _fountain.SetArrangement(DmxFountain::ArrangementFromIndex((int)event.GetPropertyValue().GetLong()));
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainArrangement");
        _fountain.AddASAPWork(OutputModelManager::WORK_RELOAD_PROPERTYGRID | OutputModelManager::WORK_RELOAD_MODELLIST,
                              "DmxFountainPropertyAdapter::OnPropertyGridChange::FountainArrangement");
        return 0;
    }
    if (name == "FountainClockwise") {
        _fountain.SetClockwise(event.GetPropertyValue().GetBool());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainClockwise");
        return 0;
    }
    if (name == "FountainNozzleSpread") {
        _fountain.SetNozzleSpread((int)event.GetPropertyValue().GetLong());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainNozzleSpread");
        return 0;
    }
    if (name == "FountainMaxHeight") {
        _fountain.SetMaxHeightFt((float)event.GetPropertyValue().GetDouble());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainMaxHeight");
        if (!_fountain.IsCenterMaxHeightSet()) {
            // the center height follows: show its new value
            _fountain.AddASAPWork(OutputModelManager::WORK_RELOAD_PROPERTYGRID, "DmxFountainPropertyAdapter::OnPropertyGridChange::FountainMaxHeight");
        }
        return 0;
    }
    if (name == "FountainCenterMaxHeight") {
        _fountain.SetCenterMaxHeightFt((float)event.GetPropertyValue().GetDouble());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainCenterMaxHeight");
        return 0;
    }
    if (name == "FountainSpray") {
        _fountain.SetSprayEnabled(event.GetPropertyValue().GetBool());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainSpray");
        return 0;
    }
    if (name == "FountainPool") {
        _fountain.SetPoolShown(event.GetPropertyValue().GetBool());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainPool");
        return 0;
    }
    if (name == "FountainDroplets") {
        _fountain.SetDropletsPerPump((int)event.GetPropertyValue().GetLong());
        redraw("DmxFountainPropertyAdapter::OnPropertyGridChange::FountainDroplets");
        return 0;
    }

    if ("DmxChannelCount" == name) {
        _fountain.IncrementChangeCount();
        _fountain.UpdateChannelCount(std::max((int)event.GetPropertyValue().GetLong(), _fountain.GetRequiredChannelCount()), true);
        return 0;
    }

    if (_fountain.HasPresetAbility() && DmxAbilityPropertyHelpers::OnPresetPropertyGridChange(grid, event, *_fountain.GetPresetAbility(), _fountain.GetDmxChannelCount(), &_fountain) == 0) {
        _fountain.IncrementChangeCount();
        return 0;
    }

    return ModelPropertyAdapter::OnPropertyGridChange(grid, event);
}
