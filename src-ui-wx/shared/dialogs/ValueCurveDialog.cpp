/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

//(*InternalHeaders(ValueCurveDialog)
#include <wx/intl.h>
#include <wx/string.h>
//*)

#include <wx/dcbuffer.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>
#include <wx/valnum.h>

#include "utils/ExternalHooks.h"
#include "UtilFunctions.h"
#include "shared/utils/wxUtilities.h"
#include "render/ValueCurve.h"
#include "color/xlColourData.h"
#include "ValueCurveDialog.h"
#include "shared/utils/ValueCurveRendering.h"
#include "xLightsApp.h"
#include "xLightsMain.h"
#include "xLightsVersion.h"
#include "sequencer/MainSequencer.h"
#include "render/SequenceElements.h"

#include <log.h>
#include <spdlog/fmt/fmt.h>
#include <algorithm>
#include <optional>
#include <cmath>

BEGIN_EVENT_TABLE(ValueCurvePanel, wxWindow)
EVT_MOTION(ValueCurvePanel::mouseMoved)
EVT_LEFT_DOWN(ValueCurvePanel::mouseLeftDown)
EVT_LEFT_UP(ValueCurvePanel::mouseLeftUp)
EVT_ENTER_WINDOW(ValueCurvePanel::mouseEnter)
EVT_LEAVE_WINDOW(ValueCurvePanel::mouseLeave)
EVT_LEFT_DCLICK(ValueCurvePanel::mouseLeftDClick)
EVT_PAINT(ValueCurvePanel::Paint)
EVT_MOUSE_CAPTURE_LOST(ValueCurvePanel::mouseCaptureLost)
END_EVENT_TABLE()

ValueCurvePanel::ValueCurvePanel(wxWindow* parent, Element* timingElement, int start, int end, wxWindowID id, const wxPoint& pos, const wxSize& size, long style) :
    wxWindow(parent, id, pos, size, style, "ID_VCP"), xlCustomControl() {
    Connect(wxEVT_LEFT_DOWN, (wxObjectEventFunction)&ValueCurvePanel::mouseLeftDown, 0, this);
    Connect(wxEVT_LEFT_UP, (wxObjectEventFunction)&ValueCurvePanel::mouseLeftUp, 0, this);
    Connect(wxEVT_ENTER_WINDOW, (wxObjectEventFunction)&ValueCurvePanel::mouseEnter, 0, this);
    Connect(wxEVT_LEAVE_WINDOW, (wxObjectEventFunction)&ValueCurvePanel::mouseLeave, 0, this);
    Connect(wxEVT_MOTION, (wxObjectEventFunction)&ValueCurvePanel::mouseMoved, 0, this);
    Connect(wxEVT_PAINT, (wxObjectEventFunction)&ValueCurvePanel::Paint, 0, this);
    Connect(wxEVT_MOUSE_CAPTURE_LOST, (wxObjectEventFunction)&ValueCurvePanel::mouseCaptureLost, 0, this);
    Connect(wxEVT_LEFT_DCLICK, (wxObjectEventFunction)&ValueCurvePanel::mouseLeftDClick, 0, this);
    Connect(wxEVT_RIGHT_DOWN, (wxObjectEventFunction)&ValueCurvePanel::mouseRightDown, 0, this);
    // ReSharper disable CppVirtualFunctionCallInsideCtor
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    // ReSharper restore CppVirtualFunctionCallInsideCtor
    _grabbedPoint = -1;
    _start = start;
    _end = end;
    _timingElement = timingElement;
}

// the left margin (_leftMargin, sized to the labels) holds the values; top and bottom leave room for the max / min
// label text
#define X_VC_RIGHT 5.0
#define Y_VC_MARGIN 9.0

void ValueCurvePanel::Convert(float& x, float& y, wxMouseEvent& event) {
    wxSize size = GetSize();
    float startX = _leftMargin;
    float startY = Y_VC_MARGIN;
    float bw = size.GetWidth() - (_leftMargin + X_VC_RIGHT);
    float bh = size.GetHeight() - 2 * Y_VC_MARGIN;

    if (event.GetX() < _leftMargin) {
        x = 0;
    } else if (event.GetX() > _leftMargin + bw) {
        x = 1.0;
    } else {
        x = (event.GetX() - startX) / bw;
    }
    if (event.GetY() < Y_VC_MARGIN) {
        y = 1.0;
    } else if (event.GetY() > Y_VC_MARGIN + bh) {
        y = 0.0;
    } else {
        y = 1.0 - (event.GetY() - startY) / bh;
    }
    y /= ViewScale(); // the graph's top is the view max
}

//(*IdInit(ValueCurveDialog)
const long ValueCurveDialog::ID_STATICTEXT3 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT7 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT4 = wxNewId();
const long ValueCurveDialog::ID_CHOICE1 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT1 = wxNewId();
const long ValueCurveDialog::ID_SLIDER_Parameter1 = wxNewId();
const long ValueCurveDialog::IDD_TEXTCTRL_Parameter1 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT2 = wxNewId();
const long ValueCurveDialog::ID_SLIDER_Parameter2 = wxNewId();
const long ValueCurveDialog::IDD_TEXTCTRL_Parameter2 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT5 = wxNewId();
const long ValueCurveDialog::ID_SLIDER_Parameter3 = wxNewId();
const long ValueCurveDialog::IDD_TEXTCTRL_Parameter3 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT6 = wxNewId();
const long ValueCurveDialog::ID_SLIDER_Parameter4 = wxNewId();
const long ValueCurveDialog::IDD_TEXTCTRL_Parameter4 = wxNewId();
const long ValueCurveDialog::ID_CHECKBOX_WrapValues = wxNewId();
const long ValueCurveDialog::ID_BUTTON5 = wxNewId();
const long ValueCurveDialog::ID_BUTTON6 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT8 = wxNewId();
const long ValueCurveDialog::ID_SLIDER1 = wxNewId();
const long ValueCurveDialog::ID_TEXTCTRL1 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT9 = wxNewId();
const long ValueCurveDialog::ID_CHOICE2 = wxNewId();
const long ValueCurveDialog::ID_STATICTEXT10 = wxNewId();
const long ValueCurveDialog::ID_TEXTCTRL2 = wxNewId();
const long ValueCurveDialog::ID_CHECKBOX1 = wxNewId();
const long ValueCurveDialog::ID_BUTTON3 = wxNewId();
const long ValueCurveDialog::ID_BUTTON4 = wxNewId();
const long ValueCurveDialog::ID_BUTTON1 = wxNewId();
const long ValueCurveDialog::ID_BUTTON2 = wxNewId();
//*)

// Audio track ID (outside wxSmith guard)
const long ValueCurveDialog::ID_CHOICE_AudioTrack = wxNewId();

BEGIN_EVENT_TABLE(ValueCurveDialog, wxDialog)
//(*EventTable(ValueCurveDialog)
//*)
END_EVENT_TABLE()

ValueCurveDialog::ValueCurveDialog(wxWindow* parent, ValueCurve* vc, bool slideridd, SequenceElements* sequenceElements, wxWindowID id, const wxPoint& pos, const wxSize& size) {
    _vc = vc;
    _slideridd = slideridd;

    //(*Initialize(ValueCurveDialog)
    wxFlexGridSizer* FlexGridSizer1;
    wxFlexGridSizer* FlexGridSizer2;
    wxFlexGridSizer* FlexGridSizer3;
    wxFlexGridSizer* FlexGridSizer4;
    wxFlexGridSizer* FlexGridSizer5;
    wxFlexGridSizer* FlexGridSizer6;
    wxFlexGridSizer* FlexGridSizer7;
    wxFlexGridSizer* FlexGridSizer8;
    wxFlexGridSizer* FlexGridSizer9;

    Create(parent, id, _("Value Curve"), wxDefaultPosition, wxDefaultSize, wxCAPTION|wxRESIZE_BORDER|wxCLOSE_BOX, _T("id"));
    SetClientSize(wxDefaultSize);
    Move(wxDefaultPosition);
    FlexGridSizer1 = new wxFlexGridSizer(0, 1, 0, 0);
    FlexGridSizer1->AddGrowableCol(0);
    FlexGridSizer1->AddGrowableRow(0);
    FlexGridSizer6 = new wxFlexGridSizer(0, 3, 0, 0);
    FlexGridSizer6->AddGrowableCol(1);
    FlexGridSizer6->AddGrowableRow(0);
    FlexGridSizer5 = new wxFlexGridSizer(3, 1, 0, 0);
    FlexGridSizer5->AddGrowableCol(0);
    FlexGridSizer5->AddGrowableRow(1);
    StaticText_TopValue = new wxStaticText(this, ID_STATICTEXT3, _("100"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT, _T("ID_STATICTEXT3"));
    FlexGridSizer5->Add(StaticText_TopValue, 1, wxALL|wxEXPAND, 2);
    StaticText1 = new wxStaticText(this, ID_STATICTEXT7, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT7"));
    FlexGridSizer5->Add(StaticText1, 1, wxALL|wxEXPAND, 5);
    StaticText_BottomValue = new wxStaticText(this, ID_STATICTEXT4, _("0"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT, _T("ID_STATICTEXT4"));
    FlexGridSizer5->Add(StaticText_BottomValue, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer6->Add(FlexGridSizer5, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer4 = new wxFlexGridSizer(0, 2, 0, 0);
    FlexGridSizer4->AddGrowableCol(0);
    FlexGridSizer4->AddGrowableRow(0);
    FlexGridSizer6->Add(FlexGridSizer4, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer1->Add(FlexGridSizer6, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer2 = new wxFlexGridSizer(0, 3, 0, 0);
    FlexGridSizer2->AddGrowableCol(1);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    Choice1 = new wxChoice(this, ID_CHOICE1, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
    Choice1->SetSelection( Choice1->Append(_("Flat")) );
    Choice1->Append(_("Ramp"));
    Choice1->Append(_("Ramp Up/Down"));
    Choice1->Append(_("Ramp Up/Down Hold"));
    Choice1->Append(_("Saw Tooth"));
    Choice1->Append(_("Parabolic Down"));
    Choice1->Append(_("Parabolic Up"));
    Choice1->Append(_("Logarithmic Up"));
    Choice1->Append(_("Logarithmic Down"));
    Choice1->Append(_("Exponential Up"));
    Choice1->Append(_("Exponential Down"));
    Choice1->Append(_("Sine"));
    Choice1->Append(_("Abs Sine"));
    Choice1->Append(_("Decaying Sine"));
    Choice1->Append(_("Square"));
    Choice1->Append(_("Random"));
    Choice1->Append(_("Music"));
    Choice1->Append(_("Inverted Music"));
    Choice1->Append(_("Music Trigger Fade"));
    Choice1->Append(_("Timing Track Toggle"));
    Choice1->Append(_("Timing Track Fade Fixed"));
    Choice1->Append(_("Timing Track Fade Proportional"));
    Choice1->Append(_("Custom"));
    FlexGridSizer2->Add(Choice1, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticText_P1 = new wxStaticText(this, ID_STATICTEXT1, _("XXXXXXXXXXXXXXX"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
    FlexGridSizer2->Add(StaticText_P1, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    Slider_Parameter1 = new wxSlider(this, ID_SLIDER_Parameter1, 0, 0, 100, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_SLIDER_Parameter1"));
    FlexGridSizer2->Add(Slider_Parameter1, 1, wxALL|wxEXPAND, 2);
    TextCtrl_Parameter1 = new wxTextCtrl(this, IDD_TEXTCTRL_Parameter1, _("0"), wxDefaultPosition, wxDLG_UNIT(this,wxSize(30,-1)), 0, wxDefaultValidator, _T("IDD_TEXTCTRL_Parameter1"));
    TextCtrl_Parameter1->SetMaxLength(6);
    FlexGridSizer2->Add(TextCtrl_Parameter1, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    StaticText_P2 = new wxStaticText(this, ID_STATICTEXT2, _("Parameter 2"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
    FlexGridSizer2->Add(StaticText_P2, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    Slider_Parameter2 = new wxSlider(this, ID_SLIDER_Parameter2, 0, 0, 100, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_SLIDER_Parameter2"));
    FlexGridSizer2->Add(Slider_Parameter2, 1, wxALL|wxEXPAND, 2);
    TextCtrl_Parameter2 = new wxTextCtrl(this, IDD_TEXTCTRL_Parameter2, _("0"), wxDefaultPosition, wxDLG_UNIT(this,wxSize(30,-1)), 0, wxDefaultValidator, _T("IDD_TEXTCTRL_Parameter2"));
    TextCtrl_Parameter2->SetMaxLength(6);
    FlexGridSizer2->Add(TextCtrl_Parameter2, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    StaticText_P3 = new wxStaticText(this, ID_STATICTEXT5, _("Parameter 3"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT5"));
    FlexGridSizer2->Add(StaticText_P3, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    Slider_Parameter3 = new wxSlider(this, ID_SLIDER_Parameter3, 0, 0, 100, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_SLIDER_Parameter3"));
    FlexGridSizer2->Add(Slider_Parameter3, 1, wxALL|wxEXPAND, 2);
    TextCtrl_Parameter3 = new wxTextCtrl(this, IDD_TEXTCTRL_Parameter3, _("0"), wxDefaultPosition, wxDLG_UNIT(this,wxSize(30,-1)), 0, wxDefaultValidator, _T("IDD_TEXTCTRL_Parameter3"));
    TextCtrl_Parameter3->SetMaxLength(6);
    FlexGridSizer2->Add(TextCtrl_Parameter3, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    StaticText_P4 = new wxStaticText(this, ID_STATICTEXT6, _("Label"), wxDefaultPosition, wxSize(121,16), 0, _T("ID_STATICTEXT6"));
    FlexGridSizer2->Add(StaticText_P4, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    Slider_Parameter4 = new wxSlider(this, ID_SLIDER_Parameter4, 0, 0, 100, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_SLIDER_Parameter4"));
    FlexGridSizer2->Add(Slider_Parameter4, 1, wxALL|wxEXPAND, 2);
    TextCtrl_Parameter4 = new wxTextCtrl(this, IDD_TEXTCTRL_Parameter4, _("0"), wxDefaultPosition, wxDLG_UNIT(this,wxSize(30,-1)), 0, wxDefaultValidator, _T("IDD_TEXTCTRL_Parameter4"));
    TextCtrl_Parameter4->SetMaxLength(6);
    FlexGridSizer2->Add(TextCtrl_Parameter4, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    CheckBox_WrapValues = new wxCheckBox(this, ID_CHECKBOX_WrapValues, _("Wrap Values"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX_WrapValues"));
    CheckBox_WrapValues->SetValue(false);
    FlexGridSizer2->Add(CheckBox_WrapValues, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 2);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer9 = new wxFlexGridSizer(0, 3, 0, 0);
    Button_Reverse = new wxButton(this, ID_BUTTON5, _("Reverse"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON5"));
    FlexGridSizer9->Add(Button_Reverse, 1, wxALL|wxEXPAND, 2);
    Button_Flip = new wxButton(this, ID_BUTTON6, _("Flip"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON6"));
    FlexGridSizer9->Add(Button_Flip, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 2);
    FlexGridSizer2->Add(FlexGridSizer9, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticText2 = new wxStaticText(this, ID_STATICTEXT8, _("Time Offset:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT8"));
    FlexGridSizer2->Add(StaticText2, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
    Slider_TimeOffset = new wxSlider(this, ID_SLIDER1, 0, 0, 100, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_SLIDER1"));
    FlexGridSizer2->Add(Slider_TimeOffset, 1, wxALL|wxEXPAND, 5);
    TextCtrl_TimeOffset = new wxTextCtrl(this, ID_TEXTCTRL1, _("0"), wxDefaultPosition, wxDLG_UNIT(this,wxSize(25,-1)), 0, wxDefaultValidator, _T("ID_TEXTCTRL1"));
    TextCtrl_TimeOffset->SetMaxLength(5);
    FlexGridSizer2->Add(TextCtrl_TimeOffset, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticText3 = new wxStaticText(this, ID_STATICTEXT9, _("Timing Track:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT9"));
    FlexGridSizer2->Add(StaticText3, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
    Choice_TimingTrack = new wxChoice(this, ID_CHOICE2, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE2"));
    FlexGridSizer2->Add(Choice_TimingTrack, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer2->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    StaticText4 = new wxStaticText(this, ID_STATICTEXT10, _("Filter Label:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT10"));
    FlexGridSizer2->Add(StaticText4, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
    TextCtrl_FilterLabel = new wxTextCtrl(this, ID_TEXTCTRL2, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TEXTCTRL2"));
    FlexGridSizer2->Add(TextCtrl_FilterLabel, 1, wxALL|wxEXPAND, 5);
    CheckBox_FilterLabelRegex = new wxCheckBox(this, ID_CHECKBOX1, _("Regex"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_CHECKBOX1"));
    CheckBox_FilterLabelRegex->SetValue(false);
    FlexGridSizer2->Add(CheckBox_FilterLabelRegex, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer1->Add(FlexGridSizer2, 1, wxALL|wxEXPAND, 5);
    FlexGridSizer7 = new wxFlexGridSizer(0, 2, 0, 0);
    FlexGridSizer7->AddGrowableCol(0);
    FlexGridSizer7->AddGrowableRow(0);
    PresetSizer = new wxFlexGridSizer(0, 5, 0, 0);
    FlexGridSizer7->Add(PresetSizer, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer8 = new wxFlexGridSizer(0, 2, 0, 0);
    ButtonLoad = new wxButton(this, ID_BUTTON3, _("Load"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON3"));
    FlexGridSizer8->Add(ButtonLoad, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    ButtonExport = new wxButton(this, ID_BUTTON4, _("Export"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON4"));
    FlexGridSizer8->Add(ButtonExport, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer7->Add(FlexGridSizer8, 1, wxALL|wxEXPAND, 2);
    FlexGridSizer1->Add(FlexGridSizer7, 1, wxALL|wxEXPAND, 1);
    FlexGridSizer3 = new wxFlexGridSizer(0, 3, 0, 0);
    Button_Ok = new wxButton(this, ID_BUTTON1, _("Ok"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON1"));
    FlexGridSizer3->Add(Button_Ok, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    Button_Cancel = new wxButton(this, ID_BUTTON2, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON2"));
    FlexGridSizer3->Add(Button_Cancel, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    FlexGridSizer1->Add(FlexGridSizer3, 1, wxALL|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 2);
    SetSizer(FlexGridSizer1);
    FlexGridSizer1->Fit(this);
    FlexGridSizer1->SetSizeHints(this);

    Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&ValueCurveDialog::OnChoice1Select);
    Connect(ID_SLIDER_Parameter1,wxEVT_COMMAND_SLIDER_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnSlider_Parameter1CmdSliderUpdated);
    Connect(IDD_TEXTCTRL_Parameter1,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnTextCtrl_Parameter1Text);
    Connect(ID_SLIDER_Parameter2,wxEVT_COMMAND_SLIDER_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnSlider_Parameter2CmdSliderUpdated);
    Connect(IDD_TEXTCTRL_Parameter2,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnTextCtrl_Parameter2Text);
    Connect(ID_SLIDER_Parameter3,wxEVT_COMMAND_SLIDER_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnSlider_Parameter3CmdSliderUpdated);
    Connect(IDD_TEXTCTRL_Parameter3,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnTextCtrl_Parameter3Text);
    Connect(ID_SLIDER_Parameter4,wxEVT_COMMAND_SLIDER_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnSlider_Parameter4CmdSliderUpdated);
    Connect(IDD_TEXTCTRL_Parameter4,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnTextCtrl_Parameter4Text);
    Connect(ID_CHECKBOX_WrapValues,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnCheckBox_WrapValuesClick);
    Connect(ID_BUTTON5,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnButton_ReverseClick);
    Connect(ID_BUTTON6,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnButton_FlipClick);
    Connect(ID_SLIDER1,wxEVT_COMMAND_SLIDER_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnSlider_TimeOffsetCmdSliderUpdated);
    Connect(ID_TEXTCTRL1,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnTextCtrl_TimeOffsetText);
    Connect(ID_CHOICE2,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&ValueCurveDialog::OnChoice_TimingTrackSelect);
    Connect(ID_TEXTCTRL2,wxEVT_COMMAND_TEXT_UPDATED,(wxObjectEventFunction)&ValueCurveDialog::OnTextCtrl_FilterLabelText);
    Connect(ID_CHECKBOX1,wxEVT_COMMAND_CHECKBOX_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnCheckBox_FilterLabelRegexClick);
    Connect(ID_BUTTON3,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnButtonLoadClick);
    Connect(ID_BUTTON4,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnButtonExportClick);
    Connect(ID_BUTTON1,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnButton_OkClick);
    Connect(ID_BUTTON2,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ValueCurveDialog::OnButton_CancelClick);
    Connect(wxEVT_SIZE,(wxObjectEventFunction)&ValueCurveDialog::OnResize);
    //*)

    Connect(wxID_ANY, wxEVT_CHAR_HOOK, wxKeyEventHandler(ValueCurveDialog::OnChar), (wxObject*)nullptr, this);

    // Audio track row — added as a proper 3-column row in FlexGridSizer2,
    // matching the Timing Track and Filter Label rows above it.
    {
        StaticText_AudioTrack = new wxStaticText(this, wxID_ANY, _("Audio Track:"), wxDefaultPosition, wxDefaultSize, 0);
        FlexGridSizer2->Add(StaticText_AudioTrack, 1, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 5);
        Choice_AudioTrack = new wxChoice(this, ID_CHOICE_AudioTrack, wxDefaultPosition, wxDefaultSize, 0, nullptr, 0);
        FlexGridSizer2->Add(Choice_AudioTrack, 1, wxALL | wxEXPAND, 5);
        FlexGridSizer2->Add(-1, -1, 1, wxALL | wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 5);
        Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            int sel = Choice_AudioTrack->GetSelection();
            _vc->SetAudioTrack(sel > 0 ? Choice_AudioTrack->GetStringSelection().ToStdString() : "");
        }, ID_CHOICE_AudioTrack);
    }

    TextCtrl_FilterLabel->SetToolTip("Only trigger on timing events which contain this token in their text. Blank matches all. Multiple tokens can be ; separated in non-regex mode.");

    Button_Ok->SetDefault();
    SetEscapeId(ID_BUTTON2);

    int start = -1;
    int end = -1;
    Effect* eff = xLightsApp::GetFrame()->GetMainSequencer()->GetSelectedEffect();
    if (eff != nullptr) {
        start = eff->GetStartTimeMS();
        end = eff->GetEndTimeMS();
    }

    for (int i = 0; i < sequenceElements->GetNumberOfTimingElements(); i++) {
        auto te = sequenceElements->GetTimingElement(i);
        Choice_TimingTrack->Append(te->GetName());
    }

    Element* timingElement = xLightsApp::GetFrame()->GetMainSequencer()->PanelEffectGrid->GetActiveTimingElement();

    _vcp = new ValueCurvePanel(this, timingElement, start, end, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    _vcp->SetMinSize(wxSize(200, 100));
    _vcp->SetValue(_vc);
    _vcp->SetType(_vc->GetType());
    _vcp->SetTimeOffset(_vc->GetTimeOffset());
    // the graph, and under it the colour bar when the caller allows colour
    _graphSizer = new wxBoxSizer(wxVERTICAL);
    _graphSizer->Add(_vcp, 1, wxEXPAND);
    FlexGridSizer4->Add(_graphSizer, 1, wxALL | wxEXPAND, 2);

    // Graph Max: a row under the curve type. Lowering it only makes editing simpler (the graph, the sliders that
    // span the whole range and the point values stop there); it is saved with the curve.
    // laid out like the parameter rows: label, slider, text box. The slider steps in the curve's own units (as the
    // parameter sliders do), from just above Min to the real Max (the full range).
    const wxString graphMaxTip = wxString::Format("The top of the graph and of the sliders, up to %s (the full range).", _vcp->FormatValue(_vc->GetMax()));
    _viewMaxSlider = new wxSlider(this, wxID_ANY, (int)std::lround(_vc->GetViewMax()), (int)std::floor(_vc->GetMin()) + 1,
                                  std::max((int)std::floor(_vc->GetMin()) + 2, (int)std::lround(_vc->GetMax())));
    _viewMaxSlider->SetToolTip(graphMaxTip);
    _viewMaxText = new wxTextCtrl(this, wxID_ANY, _vcp->FormatValue(_vc->GetViewMax()), wxDefaultPosition, TextCtrl_Parameter1->GetSize(), wxTE_PROCESS_ENTER);
    _viewMaxText->SetToolTip(graphMaxTip);
    // dragging moves the graph's top as it goes (it changes no output, so nothing re-renders)
    _viewMaxSlider->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) { SetViewMax((float)_viewMaxSlider->GetValue()); });
    _viewMaxText->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { ApplyViewMax(); });
    _viewMaxText->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) { ApplyViewMax(); e.Skip(); });
    FlexGridSizer2->Insert(3, new wxStaticText(this, wxID_ANY, "Graph Max"), 1, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 2);
    FlexGridSizer2->Insert(4, _viewMaxSlider, 1, wxALL | wxEXPAND, 2);
    FlexGridSizer2->Insert(5, _viewMaxText, 1, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 2);

    // Edit Points / Cycles Preview: only for a custom curve with more than one cycle
    auto* cyclesLabel = new wxStaticText(this, wxID_ANY, "Cycles");
    auto* cyclesButtons = new wxPanel(this);
    auto* cyclesSizer = new wxBoxSizer(wxHORIZONTAL);
    _editPointsButton = new wxToggleButton(cyclesButtons, wxID_ANY, "Edit Points");
    _cyclesPreviewButton = new wxToggleButton(cyclesButtons, wxID_ANY, "Cycles Preview");
    _editPointsButton->SetToolTip("Edit the points of one cycle; every cycle follows.");
    _cyclesPreviewButton->SetToolTip("See the cycles repeated across the curve.");
    cyclesSizer->Add(_editPointsButton, 0, wxRIGHT, 4);
    cyclesSizer->Add(_cyclesPreviewButton, 0);
    cyclesButtons->SetSizer(cyclesSizer);
    auto* cyclesSpacer = new wxStaticText(this, wxID_ANY, wxEmptyString);
    FlexGridSizer2->Insert(6, cyclesLabel, 1, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 2);
    FlexGridSizer2->Insert(7, cyclesButtons, 1, wxALL | wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 2);
    FlexGridSizer2->Insert(8, cyclesSpacer, 1, wxALL, 2);
    _cyclesRow = { cyclesLabel, cyclesButtons, cyclesSpacer };
    _editPointsButton->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { _cyclesPreview = false; UpdateCyclesMode(); });
    _cyclesPreviewButton->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { _cyclesPreview = true; UpdateCyclesMode(); });
    // edits on the one-cycle copy go back to the curve as its cycle
    _vcp->SetEditedCallback([this]() {
        if (_editVc) {
            _vc->SetBaseCustomPoints(_editVc->GetPoints());
        }
    });
    // the panel draws the min / max (and the lines between) beside its grid
    StaticText_TopValue->Hide();
    StaticText_BottomValue->Hide();
    // ReSharper disable once CppVirtualFunctionCallInsideCtor
    Layout();
    // ReSharper disable once CppVirtualFunctionCallInsideCtor
    Fit();

    _backup = *_vc;

    wxString sMin; // = "";
    wxString sMax; // = "";

    switch (_vc->GetDivisor()) {
    case 360:
        sMin = wxString::Format("%.2f", _vc->GetMin() / 360.0);
        sMax = wxString::Format("%.2f", _vc->GetMax() / 360.0);
        break;
    case 100:
        sMin = wxString::Format("%.2f", _vc->GetMin() / 100.0);
        sMax = wxString::Format("%.2f", _vc->GetMax() / 100.0);
        break;
    case 10:
        sMin = wxString::Format("%.1f", _vc->GetMin() / 10.0);
        sMax = wxString::Format("%.1f", _vc->GetMax() / 10.0);
        break;
    default:
        sMin = wxString::Format("%i", (int)_vc->GetMin());
        sMax = wxString::Format("%i", (int)_vc->GetMax());
        break;
    }

    SetSliderMinMax();

    StaticText_BottomValue->SetLabel(sMin);
    StaticText_TopValue->SetLabel(sMax);

    CheckBox_WrapValues->SetValue(_vc->GetWrap());
    Choice1->SetStringSelection(wxString(_vc->GetType().c_str()));
    Choice_TimingTrack->SetStringSelection(_vc->GetTimingTrack());
    Slider_TimeOffset->SetValue(_vc->GetTimeOffset());
    SetParameter(1, _vc->GetParameter1());
    SetParameter(2, _vc->GetParameter2());
    SetParameter(3, _vc->GetParameter3());
    SetParameter(4, _vc->GetParameter4());
    SetTextCtrlsFromSliders();
    Choice1->SetFocus();
    TextCtrl_FilterLabel->SetValue(_vc->GetFilterLabelText());
    CheckBox_FilterLabelRegex->SetValue(_vc->IsFilterLabelRegex());

    PopulatePresets();

    // Populate audio track choice with available alt tracks
    Choice_AudioTrack->Clear();
    Choice_AudioTrack->Append(_("(Main Track)"));
    {
        auto* seqFile = xLightsApp::GetFrame()->CurrentSeqXmlFile;
        if (seqFile != nullptr) {
            for (int i = 0; i < seqFile->GetAltTrackCount(); i++) {
                Choice_AudioTrack->Append(wxString(seqFile->GetAltTrackDisplayName(i)));
            }
        }
    }
    // Restore saved selection
    if (!_vc->GetAudioTrack().empty()) {
        if (Choice_AudioTrack->SetStringSelection(wxString(_vc->GetAudioTrack())) == false) {
            Choice_AudioTrack->SetSelection(0);
        }
    } else {
        Choice_AudioTrack->SetSelection(0);
    }

    Layout();
    Fit();

    wxSize sz = GetSize();

    if (sz.x < 500) {
        sz.x = 800;
    }
    if (sz.y < 400) {
        sz.y = 600;
    }
    SetSize(sz);

    ValidateWindow();
}

ValueCurveDialog::~ValueCurveDialog() {
    //(*Destroy(ValueCurveDialog)
    //*)
}

void ValueCurveDialog::OnButton_OkClick(wxCommandEvent& event) {
    _liveTimer.Stop();
    _vc->SetRealValue();
    EndDialog(wxOK);
}

void ValueCurveDialog::OnButton_CancelClick(wxCommandEvent& event) {
    _liveTimer.Stop();
    *_vc = _backup;
    if (_liveChange && LiveSignature() != _liveSig) {
        _liveChange(); // the preview goes back to the original curve
    }
    EndDialog(wxCANCEL);
}

// The colour editor is a simpler dialog: always a custom curve of one cycle over the full range, with no time
// offset or timing track, so all that shows is the graph, the colour bar, Wrap Values and OK / Cancel.
void ValueCurveDialog::EnableColor() {
    if (_colorBar != nullptr) {
        return;
    }
    if (_vc->GetType() != "Custom") {
        _vc->SetType("Custom"); // starts from the points the old type drew
    }
    Choice1->SetStringSelection("Custom");
    _vcp->SetType("Custom");
    _vc->SetParameter1(1);
    _vc->SetTimeOffset(0);
    _vcp->SetTimeOffset(0);
    Slider_TimeOffset->SetValue(0);
    TextCtrl_TimeOffset->ChangeValue("0");
    _vc->SetTimingTrack("");
    _vc->SetViewMax(0.0f); // the full range
    SetSliderMinMax();
    ValidateWindow();

    _colorBar = new ValueCurveColorBar(this, _vcp);
    _colorBar->SetValue(_vc);
    _graphSizer->Add(_colorBar, 0, wxEXPAND | wxTOP, 6);
    // a colour key and a curve point are never selected together
    _colorBar->SetSelectedCallback([this]() { _vcp->Deselect(); });
    _vcp->SetSelectedCallback([this]() { _colorBar->Deselect(); });
    _colorBar->SetChangedCallback([this]() { PushLiveChange(); });

    for (wxWindow* w : GetChildren()) {
        if (w != _vcp && w != _colorBar && w != CheckBox_WrapValues && w != Button_Ok && w != Button_Cancel) {
            w->Hide();
        }
    }
    _vcp->SetMinSize(wxSize(600, 260));
    Layout();
    Fit();
}

void ValueCurveDialog::PushLiveChange() {
    if (_liveChange) {
        _liveSig = LiveSignature();
        _liveChange();
    }
}

// Shows Edit Points / Cycles Preview for a custom curve with more than one cycle and points the graph at the right
// curve: a one-cycle copy to edit, or the curve itself (the repeated cycles, read only).
void ValueCurveDialog::UpdateCyclesMode() {
    if (_vcp == nullptr || _editPointsButton == nullptr) {
        return; // still being built
    }
    const bool cycles = _vc->GetType() == "Custom" && std::lround(_vc->GetParameter1()) > 1;
    for (auto* w : _cyclesRow) {
        w->Show(cycles);
    }
    if (cycles && !_cyclesPreview) {
        // a fresh copy each time, so it follows Reverse, Flip, presets and the like done to the curve
        _editVc = std::make_unique<ValueCurve>(*_vc);
        _editVc->SetParameter1(1);
        _vcp->SetValue(_editVc.get());
        _vcp->ClearSelection();
    } else if (_editVc) {
        _vcp->SetValue(_vc);
        _vcp->ClearSelection();
        _editVc.reset();
    }
    if (!cycles) {
        _cyclesPreview = true; // the next time Cycles goes above 1 it starts in the preview
    }
    _editPointsButton->SetValue(cycles && !_cyclesPreview);
    _cyclesPreviewButton->SetValue(cycles && _cyclesPreview);
    Layout();
    _vcp->Refresh();
    if (_colorBar != nullptr) {
        _colorBar->Refresh(); // Reverse, Wrap, Load... change the colours too
    }
}

// Reads Graph Max: anything at or above the real max (or empty, or not above the min) means the full range.
void ValueCurveDialog::ApplyViewMax() {
    wxString text = _viewMaxText->GetValue();
    text.Trim(true).Trim(false);
    const float shown = text.IsEmpty() ? 0.0f : (float)std::strtod(text.utf8_str(), nullptr);
    SetViewMax(shown * _vc->GetDivisor());
}

// Sets Graph Max (in the curve's own units) and keeps its slider, its text box, the parameter sliders and the graph
// in step.
void ValueCurveDialog::SetViewMax(float raw) {
    const float before = _vc->GetViewMax();
    _vc->SetViewMax(raw);
    _viewMaxText->ChangeValue(_vcp->FormatValue(_vc->GetViewMax()));
    _viewMaxSlider->SetValue((int)std::lround(_vc->GetViewMax()));
    if (_vc->GetViewMax() != before) {
        SetSliderMinMax();
        ValidateWindow();
        _vcp->Refresh();
    }
}

void ValueCurveDialog::SetLiveChangeCallback(std::function<void()> onChange) {
    _liveChange = std::move(onChange);
    _liveSig = LiveSignature();
    _liveTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &ValueCurveDialog::OnLiveTimer, this, _liveTimer.GetId());
    _liveTimer.Start(150);
}

// Everything about the curve that changes its output, without serialising it (which tidies the points).
std::string ValueCurveDialog::LiveSignature() const {
    std::string s = fmt::format("{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|", _vc->GetType(), _vc->IsActive(), _vc->GetParameter1(), _vc->GetParameter2(),
                                _vc->GetParameter3(), _vc->GetParameter4(), _vc->GetTimeOffset(), _vc->GetWrap(), _vc->GetTimingTrack(),
                                _vc->GetFilterLabelText(), _vc->IsFilterLabelRegex());
    for (const auto& p : _vc->GetPoints()) {
        s += fmt::format("{}:{}:{}:{}:{}:{}:{};", p.x, p.y, (int)p.shape, p.inDx, p.inDy, p.outDx, p.outDy);
    }
    s += "|";
    for (const auto& k : _vc->GetColorKeys()) {
        s += fmt::format("{}:{}:{}:{}:{};", k.x, k.color.red, k.color.green, k.color.blue, k.blend);
    }
    return s;
}

void ValueCurveDialog::OnLiveTimer(wxTimerEvent& event) {
    if (!_liveChange || wxGetMouseState().LeftIsDown()) {
        return; // mid-drag: wait for the release
    }
    std::string sig = LiveSignature();
    if (sig != _liveSig) {
        _liveSig = sig;
        _liveChange();
    }
}

void ValueCurveDialog::SetParameter100(int p, float v) {
    float low, high;
    _vc->GetRangeParm(p, Choice1->GetStringSelection().ToStdString(), low, high);

    if (low == MINVOID) {
        low = _vc->GetMin();
    }
    if (high == MAXVOID) {
        high = _vc->GetViewMax();
    }

    float vv = (high - low) * v / 100.0 + low;
    SetParameter(p, vv);
}

void ValueCurveDialog::SetParameter(int p, float v) {
    switch (p) {
    case 1:
        _vc->SetParameter1(v);
        Slider_Parameter1->SetValue(std::round(v));
        break;
    case 2:
        _vc->SetParameter2(v);
        Slider_Parameter2->SetValue(std::round(v));
        break;
    case 3:
        _vc->SetParameter3(v);
        Slider_Parameter3->SetValue(std::round(v));
        break;
    case 4:
        _vc->SetParameter4(v);
        Slider_Parameter4->SetValue(std::round(v));
        break;
    default:
        break;
    }
    SetTextCtrlsFromSliders();
}

static int computePageSize(double min, double max) {
    return std::max(1, (int)std::round((max - min) / 10.0));
}

void ValueCurveDialog::SetSliderMinMax() {
    float min, max;

    ValueCurve::GetRangeParm1(_vc->GetType(), min, max);
    if (min == MINVOID) {
        Slider_Parameter1->SetMin(_vc->GetMin());
        Slider_Parameter1->SetMax(_vc->GetViewMax());
        Slider_Parameter1->SetPageSize(computePageSize(_vc->GetMin(), _vc->GetViewMax()));
    } else {
        Slider_Parameter1->SetMin(min);
        Slider_Parameter1->SetMax(max);
        Slider_Parameter1->SetPageSize(computePageSize(min, max));
    }

    ValueCurve::GetRangeParm2(_vc->GetType(), min, max);
    if (min == MINVOID) {
        Slider_Parameter2->SetMin(_vc->GetMin());
        Slider_Parameter2->SetMax(_vc->GetViewMax());
        Slider_Parameter2->SetPageSize(computePageSize(_vc->GetMin(), _vc->GetViewMax()));
    } else {
        Slider_Parameter2->SetMin(min);
        Slider_Parameter2->SetMax(max);
        Slider_Parameter2->SetPageSize(computePageSize(min, max));
    }

    ValueCurve::GetRangeParm3(_vc->GetType(), min, max);
    if (min == MINVOID) {
        Slider_Parameter3->SetMin(_vc->GetMin());
        Slider_Parameter3->SetMax(_vc->GetViewMax());
        Slider_Parameter3->SetPageSize(computePageSize(_vc->GetMin(), _vc->GetViewMax()));
    } else {
        Slider_Parameter3->SetMin(min);
        Slider_Parameter3->SetMax(max);
        Slider_Parameter3->SetPageSize(computePageSize(min, max));
    }
    ValueCurve::GetRangeParm4(_vc->GetType(), min, max);
    if (min == MINVOID) {
        Slider_Parameter4->SetMin(_vc->GetMin());
        Slider_Parameter4->SetMax(_vc->GetViewMax());
        Slider_Parameter4->SetPageSize(computePageSize(_vc->GetMin(), _vc->GetViewMax()));
    } else {
        Slider_Parameter4->SetMin(min);
        Slider_Parameter4->SetMax(max);
        Slider_Parameter4->SetPageSize(computePageSize(min, max));
    }
}

static bool IsStartEndLevelType(const std::string& type)
{
    return type == "Exponential Up" || type == "Exponential Down" ||
           type == "Logarithmic Up" || type == "Logarithmic Down" ||
           type == "Parabolic Down" || type == "Parabolic Up";
}

void ValueCurveDialog::OnChoice1Select(wxCommandEvent& event) {
    _vcp->SetType(std::string(Choice1->GetStringSelection().c_str()));
    _vcp->SetTimeOffset(Slider_TimeOffset->GetValue());
    _vc->SetType(std::string(Choice1->GetStringSelection().c_str()));
    _vcp->Refresh();
    ValidateWindow();

    SetSliderMinMax();

    wxString type = Choice1->GetStringSelection();
    _vc->SetStartEndLevelActive(IsStartEndLevelType(type.ToStdString()));
    if (type == "Flat") {
        // Dont change anything
    } else if (type == "Ramp") {
        if (_vc->GetParameter1_100() < 50) {
            SetParameter100(2, 100);
        } else {
            SetParameter100(2, 0);
        }
    } else if (type == "Ramp Up/Down") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 0);
    } else if (type == "Music" || type == "Inverted Music") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 50);
    } else if (type == "Music Trigger Fade") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 50);
        SetParameter100(4, 10);
    } else if (type == "Timing Track Toggle") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        if (Choice_TimingTrack->GetCount() > 0) {
            Choice_TimingTrack->SetSelection(0);
            _vc->SetTimingTrack(Choice_TimingTrack->GetStringSelection());
        }
    } else if (type == "Timing Track Fade Fixed") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter(3, 10);
        if (Choice_TimingTrack->GetCount() > 0) {
            Choice_TimingTrack->SetSelection(0);
            _vc->SetTimingTrack(Choice_TimingTrack->GetStringSelection());
        }
    } else if (type == "Timing Track Fade Proportional") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter(3, 50);
        if (Choice_TimingTrack->GetCount() > 0) {
            Choice_TimingTrack->SetSelection(0);
            _vc->SetTimingTrack(Choice_TimingTrack->GetStringSelection());
        }
    } else if (type == "Ramp Up/Down Hold") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 80);
    } else if (type == "Saw Tooth") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 2);
    } else if (type == "Parabolic Down") {
        SetParameter100(1, 4);
        SetParameter100(2, 0);
        SetParameter100(3, 0);
        SetParameter100(4, 100);
    } else if (type == "Parabolic Up") {
        SetParameter100(1, 4);
        SetParameter100(2, 100);
        SetParameter100(3, 0);
        SetParameter100(4, 100);
    } else if (type == "Logarithmic Up") {
        SetParameter100(1, 4);
        SetParameter100(2, 100);
        SetParameter100(3, 0);
        SetParameter100(4, 100);
    } else if (type == "Logarithmic Down") {
        SetParameter100(1, 15);
        SetParameter100(2, 50);
        SetParameter100(3, 0);
        SetParameter100(4, 100);
    } else if (type == "Exponential Up") {
        SetParameter100(1, 100);
        SetParameter100(2, 50);
        SetParameter100(3, 0);
        SetParameter100(4, 100);
    } else if (type == "Exponential Down") {
        SetParameter100(1, 100);
        SetParameter100(2, 50);
        SetParameter100(3, 0);
        SetParameter100(4, 100);
    } else if (type == "Sine") {
        SetParameter100(1, 75);
        _vc->SetTimeOffset(0);
        SetParameter100(2, 100);
        SetParameter100(3, 10);
        SetParameter100(4, 50);
    } else if (type == "Decaying Sine") {
        SetParameter100(1, 75);
        _vc->SetTimeOffset(0);
        SetParameter100(2, 100);
        SetParameter100(3, 10);
        SetParameter100(4, 50);
    } else if (type == "Random") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 5);
        SetParameter100(4, 0);
    } else if (type == "Abs Sine") {
        SetParameter100(1, 0);
        _vc->SetTimeOffset(0);
        SetParameter100(2, 100);
        SetParameter100(3, 10);
        SetParameter100(4, 50);
    } else if (type == "Square") {
        SetParameter100(1, 0);
        SetParameter100(2, 100);
        SetParameter100(3, 1);
        SetParameter100(4, 0);
    } else if (type == "Custom") {
        SetParameter(1, 1.0f);
        // If there are no points add some
        if (_vc->GetPoints().size() == 0) {
            _vc->SetValueAt(0.0, 0.5);
            _vc->SetValueAt(1.0, 0.5);
        }
    }

    ValidateWindow();
}

#pragma region Mouse Control

void ValueCurvePanel::Undo() {
    if (_undo.size() > 0) {
        wxRealPoint p = _undo.back();
        _undo.pop_back();

        if (p.y < 0) {
            _vc->DeletePoint(p.x);
        } else {
            _vc->SetValueAt(p.x, p.y);
        }
        NotifyEdited();
    }
}

void ValueCurvePanel::SaveUndo(float x, float y) {
    wxRealPoint p(x, y);
    _undo.push_back(p);
}

void ValueCurvePanel::SaveUndoSelected() {
    if (_vc->IsSetPoint(_grabbedPoint)) {
        SaveUndo(_grabbedPoint, _vc->GetValueAt(_grabbedPoint, 0, 1));
    } else {
        SaveUndo(_grabbedPoint, -1.0f);
    }
}

float ValueCurvePanel::ToReal(float y) {
    return (_vc->GetMin() + y * (_vc->GetMax() - _vc->GetMin())) / _vc->GetDivisor();
}

float ValueCurvePanel::ToNormalised(float y) {
    return (y * _vc->GetDivisor() - _vc->GetMin()) / (_vc->GetMax() - _vc->GetMin());
}

void ValueCurvePanel::mouseLeftDClick(wxMouseEvent& event) {
    if (_type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0) {
        const float point = PointNear(event.GetX(), event.GetY());
        if (point >= 0.0f) {
            EditPoint(point);
        }
    }
}
void ValueCurvePanel::mouseLeftDown(wxMouseEvent& event) {
    if (IsEditable() && (_dragHandle = HandleAt(event.GetX(), event.GetY())) != 0) {
        if (!HasCapture()) {
            CaptureMouse();
        }
        NotifySelected();
        Refresh();
        return;
    }
    if (_type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0) {
        float x, y;
        Convert(x, y, event);
        const float hit = PointNear(event.GetX(), event.GetY());
        if (hit >= 0.0f) {
            x = hit;
        }
        _grabbedPoint = x;
        if (_grabbedPoint < 0.0f) {
            _grabbedPoint = 0.0f;
        } else if (_grabbedPoint > 1.0f) {
            _grabbedPoint = 1.0f;
        }
        _grabbedPoint = vcSortablePoint::Normalise(_grabbedPoint);
        _originalGrabbedPoint = _grabbedPoint;
        NotifySelected();
        _minGrabbedPoint = _vc->FindMinPointLessThan(_grabbedPoint);
        _maxGrabbedPoint = _vc->FindMaxPointGreaterThan(_grabbedPoint);
        SaveUndoSelected();
        // a press can arrive while the last one still holds the mouse (its release went missing)
        if (!HasCapture()) {
            CaptureMouse();
        }
        mouseMoved(event);
        Refresh();
    }
}
void ValueCurvePanel::mouseCaptureLost(wxMouseCaptureLostEvent& event) {
    _dragHandle = 0;
    if (_type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0) {
        // lets not do anything
    }
    Refresh();
}
void ValueCurvePanel::mouseLeftUp(wxMouseEvent& event) {
    if (_dragHandle != 0) {
        _dragHandle = 0;
    } else if (_type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0 && HasCapture()) {
        float x, y;
        Convert(x, y, event);
        if (y < 0.0f) {
            y = 0.0f;
        } else if (y > 1.0f) {
            y = 1.0f;
        }
        y = SnapY(y, event);
        _vc->SetValueAt(_grabbedPoint, y);
        //_grabbedPoint = -1;
        NotifyEdited();
    }
    // always let go, even if the curve stopped being editable mid-drag
    if (HasCapture()) {
        ReleaseMouse();
    }
    Refresh();
}

void ValueCurvePanel::Delete() {
    if (_grabbedPoint > 0.0 && _grabbedPoint < 1.0) {
        _vc->DeletePoint(_grabbedPoint);
        _grabbedPoint = -1;
        NotifyEdited();
        Refresh();
    } else {
        wxBell();
    }
}

void ValueCurvePanel::mouseEnter(wxMouseEvent& event) {
    if (_type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0) {
        SetCursor(wxCURSOR_CROSS);
        mouseMoved(event);
    }
}

void ValueCurvePanel::mouseLeave(wxMouseEvent& event) {
    SetCursor(wxCURSOR_DEFAULT);
}

void ValueCurvePanel::mouseMoved(wxMouseEvent& event) {
    if (_type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0) {
        static const wxCursor s_hand(wxCURSOR_HAND);
        static const wxCursor s_cross(wxCURSOR_CROSS);

        float x, y;
        Convert(x, y, event);
        if (y < 0.0f) {
            y = 0.0f;
        } else if (y > 1.0f) {
            y = 1.0f;
        }

        if (PointNear(event.GetX(), event.GetY()) >= 0.0f || HandleAt(event.GetX(), event.GetY()) != 0) {
            SetCursor(s_hand);
        } else {
            SetCursor(s_cross);
        }

        if (HasCapture() && _dragHandle != 0) {
            DragHandle(x, y, event.ShiftDown() || event.ControlDown() || event.RawControlDown() || event.CmdDown());
            NotifyEdited();
            Refresh();
            return;
        }

        if (HasCapture()) {
            y = SnapY(y, event);
            // the point's shape goes with it when it moves
            std::optional<vcSortablePoint> shape;
            if (const vcSortablePoint* sp = _vc->FindPoint(vcSortablePoint::Normalise(_grabbedPoint))) {
                shape = *sp;
            }

            if (_originalGrabbedPoint == 0 || _originalGrabbedPoint == 1.0) {
                // dont allow x to change
            } else {
                // a point between the ends can be dragged past its neighbours; the ends stay put
                const float step = vcSortablePoint::perPoint();
                const float cur = vcSortablePoint::Normalise(_grabbedPoint);
                x = std::clamp(vcSortablePoint::Normalise(x), step, 1.0f - step);
                if (x != cur) {
                    _vc->DeletePoint(_grabbedPoint);
                    // landing on another point: carry on just past it, in the direction of the drag
                    const float dir = x > cur ? step : -step;
                    while (x > 0.0f && x < 1.0f && _vc->IsSetPoint(x)) {
                        x = vcSortablePoint::Normalise(x + dir);
                    }
                    _grabbedPoint = x > 0.0f && x < 1.0f ? x : cur;
                }
            }
            _vc->SetValueAt(_grabbedPoint, y);
            if (shape) {
                _vc->CopyPointShape(_grabbedPoint, *shape);
            }
            NotifyEdited();

            Refresh();
        }

        std::string time = "";
        if (_start != -1) {
            time = std::string(FORMATTIME((int)(_start + (_end - _start) * x))) + ", ";
        }

        if (_vc->IsSetPoint(vcSortablePoint::Normalise(x))) {
            SetToolTip(wxString::Format(wxT("%s%.2f,%.1f -> %.1f"), time, x, _vc->GetScaledValue(y), ToReal(_vc->GetPointAt(vcSortablePoint::Normalise(x)))));
        } else {
            SetToolTip(wxString::Format(wxT("%s%.2f,%.1f"), time, x, _vc->GetScaledValue(y)));
        }
    } else {
        float x, y;
        Convert(x, y, event);
        if (y < 0.0f) {
            y = 0.0f;
        } else if (y > 1.0f) {
            y = 1.0f;
        }

        std::string time = "";
        if (_start != -1) {
            time = std::string(FORMATTIME((int)(_start + (_end - _start) * x))) + ", ";
        }

        SetToolTip(wxString::Format(wxT("%s%.2f,%.1f"), time, x, _vc->GetScaledValue(y)));
    }
}
#pragma endregion Mouse Control

#pragma region Sliders and TextCtrls

void ValueCurveDialog::UpdateLinkedSlider(wxCommandEvent& event) {
    SetSlidersFromTextCtrls();
}

void ValueCurveDialog::UpdateLinkedTextCtrl(wxScrollEvent& event) {
    SetTextCtrlsFromSliders();
}

void ValueCurveDialog::SetTextCtrlFromSlider(int parm, wxTextCtrl* txt, int value) {
    std::string type = Choice1->GetStringSelection().ToStdString();
    float low, high;
    ValueCurve::GetRangeParm(parm, type, low, high);

    float v = value;
    int d = _vc->GetDivisor();
    if (low == MINVOID) {
        v /= d;
    } else {
        d = 1;
    }

    switch (d) {
    case 10:
    case 360: // this kinda sucks ... its not really the same
        txt->ChangeValue(wxString::Format("%.1f", v));
        break;
    case 100:
        txt->ChangeValue(wxString::Format("%.2f", v));
        break;
    default:
        txt->ChangeValue(wxString::Format("%d", value));
        break;
    }
}

void ValueCurveDialog::SetSliderFromTextCtrl(int parm, wxSlider* slider, float value) {
    std::string type = Choice1->GetStringSelection().ToStdString();
    float low, high;
    ValueCurve::GetRangeParm(parm, type, low, high);

    float v = value;
    int d = _vc->GetDivisor();
    if (low == MINVOID) {
        v *= d;
    }

    slider->SetValue(v);
}

void ValueCurveDialog::SetTextCtrlsFromSliders() {
    SetTextCtrlFromSlider(1, TextCtrl_Parameter1, Slider_Parameter1->GetValue());
    SetTextCtrlFromSlider(2, TextCtrl_Parameter2, Slider_Parameter2->GetValue());
    SetTextCtrlFromSlider(3, TextCtrl_Parameter3, Slider_Parameter3->GetValue());
    SetTextCtrlFromSlider(4, TextCtrl_Parameter4, Slider_Parameter4->GetValue());
    TextCtrl_TimeOffset->SetValue(wxString::Format("%d", Slider_TimeOffset->GetValue()));
}

void ValueCurveDialog::SetSlidersFromTextCtrls() {
    SetSliderFromTextCtrl(1, Slider_Parameter1, wxAtof(TextCtrl_Parameter1->GetValue()));
    SetSliderFromTextCtrl(2, Slider_Parameter2, wxAtof(TextCtrl_Parameter2->GetValue()));
    SetSliderFromTextCtrl(3, Slider_Parameter3, wxAtof(TextCtrl_Parameter3->GetValue()));
    SetSliderFromTextCtrl(4, Slider_Parameter4, wxAtof(TextCtrl_Parameter4->GetValue()));

    Slider_TimeOffset->SetValue(wxAtoi(TextCtrl_TimeOffset->GetValue()));
}

void ValueCurveDialog::OnTextCtrl_Parameter1Text(wxCommandEvent& event) {
    UpdateLinkedSlider(event);
    float i = wxAtof(TextCtrl_Parameter1->GetValue());
    float low, high;
    ValueCurve::GetRangeParm1(Choice1->GetStringSelection().ToStdString(), low, high);
    if (low == MINVOID)
        i *= _vc->GetDivisor();
    _vc->SetParameter1(i);
    _cyclesPreview = true; // changing Cycles shows what it looks like
    UpdateCyclesMode();
}

void ValueCurveDialog::OnSlider_Parameter1CmdSliderUpdated(wxScrollEvent& event) {
    int val = Slider_Parameter1->GetValue();
    UpdateLinkedTextCtrl(event);
    _vc->SetParameter1(val);
    _cyclesPreview = true; // changing Cycles shows what it looks like
    UpdateCyclesMode();
}

void ValueCurveDialog::OnTextCtrl_Parameter2Text(wxCommandEvent& event) {
    UpdateLinkedSlider(event);
    float i = wxAtof(TextCtrl_Parameter2->GetValue());
    float low, high;
    ValueCurve::GetRangeParm2(Choice1->GetStringSelection().ToStdString(), low, high);
    if (low == MINVOID)
        i *= _vc->GetDivisor();
    _vc->SetParameter2(i);
    _vcp->Refresh();
}

void ValueCurveDialog::OnSlider_Parameter2CmdSliderUpdated(wxScrollEvent& event) {
    int val = Slider_Parameter2->GetValue();
    UpdateLinkedTextCtrl(event);
    _vc->SetParameter2(val);
    _vcp->Refresh();
}

void ValueCurveDialog::OnTextCtrl_Parameter3Text(wxCommandEvent& event) {
    UpdateLinkedSlider(event);
    float i = wxAtof(TextCtrl_Parameter3->GetValue());
    float low, high;
    ValueCurve::GetRangeParm3(Choice1->GetStringSelection().ToStdString(), low, high);
    if (low == MINVOID) {
        if (IsStartEndLevelType(_vc->GetType()))
            _vc->SetStartEndLevelActive(true);
        i *= _vc->GetDivisor();
    }
    _vc->SetParameter3(i);
    _vcp->Refresh();
}

void ValueCurveDialog::OnSlider_Parameter3CmdSliderUpdated(wxScrollEvent& event) {
    int val = Slider_Parameter3->GetValue();
    UpdateLinkedTextCtrl(event);
    float low, high;
    ValueCurve::GetRangeParm3(Choice1->GetStringSelection().ToStdString(), low, high);
    if (low == MINVOID && IsStartEndLevelType(_vc->GetType()))
        _vc->SetStartEndLevelActive(true);
    _vc->SetParameter3(val);
    _vcp->Refresh();
}

void ValueCurveDialog::OnTextCtrl_Parameter4Text(wxCommandEvent& event) {
    UpdateLinkedSlider(event);
    float i = wxAtof(TextCtrl_Parameter4->GetValue());
    float low, high;
    ValueCurve::GetRangeParm4(Choice1->GetStringSelection().ToStdString(), low, high);
    if (low == MINVOID) {
        if (IsStartEndLevelType(_vc->GetType()))
            _vc->SetStartEndLevelActive(true);
        i *= _vc->GetDivisor();
    }
    _vc->SetParameter4(i);
    _vcp->Refresh();
}

void ValueCurveDialog::OnSlider_Parameter4CmdSliderUpdated(wxScrollEvent& event) {
    int val = Slider_Parameter4->GetValue();
    UpdateLinkedTextCtrl(event);
    float low, high;
    ValueCurve::GetRangeParm4(Choice1->GetStringSelection().ToStdString(), low, high);
    if (low == MINVOID && IsStartEndLevelType(_vc->GetType()))
        _vc->SetStartEndLevelActive(true);
    _vc->SetParameter4(val);
    _vcp->Refresh();
}

#pragma endregion Sliders and TextCtrls

void ValueCurvePanel::DrawTiming(wxAutoBufferedPaintDC& pdc, long timeMS) {
    wxSize s = GetSize();
    float w = s.GetWidth() - (_leftMargin + X_VC_RIGHT);
    long interval = _end - _start;
    float pos = (float)(timeMS - _start) / (float)interval;
    int x = pos * w + _leftMargin;

    pdc.SetPen(*wxBLUE);
    pdc.DrawLine(x, 0, x, s.GetHeight());
}

void ValueCurvePanel::DrawTiming(wxAutoBufferedPaintDC& pdc) {
    if (_timingElement == nullptr) {
        return;
    }

    EffectLayer* el = _timingElement->GetEffectLayer(0);

    for (int i = 0; i < el->GetEffectCount(); i++) {
        Effect* e = el->GetEffect(i);
        if (e->GetStartTimeMS() >= _start || e->GetStartTimeMS() <= _end) {
            DrawTiming(pdc, e->GetStartTimeMS());
        }
        if (e->GetEndTimeMS() >= _start || e->GetEndTimeMS() <= _end) {
            DrawTiming(pdc, e->GetEndTimeMS());
        }
    }
}

// The x of the point within a few pixels of (mx, my), the nearest if there are several; -1 if none.
float ValueCurvePanel::PointNear(int mx, int my) const {
    float best = -1.0f;
    int bestDist = 7 * 7;
    for (const auto& pt : _vc->GetPoints()) {
        const wxPoint p = ToPixel(pt.x, pt.y);
        const int d = (p.x - mx) * (p.x - mx) + (p.y - my) * (p.y - my);
        if (d < bestDist) {
            bestDist = d;
            best = pt.x;
        }
    }
    return best;
}

// Graph units per curve unit: the curve's 0-1 spans Min to the real Max, the graph's Min to the view max.
float ValueCurvePanel::ViewScale() const {
    if (_vc == nullptr) {
        return 1.0f;
    }
    const float range = _vc->GetMax() - _vc->GetMin();
    const float view = _vc->GetViewMax() - _vc->GetMin();
    return range > 0.0f && view > 0.0f ? range / view : 1.0f;
}

void ValueCurvePanel::PlotRange(int& x0, int& x1) const {
    x0 = (int)std::lround(_leftMargin);
    x1 = GetSize().GetWidth() - (int)X_VC_RIGHT;
}

bool ValueCurvePanel::IsEditable() const {
    return _vc != nullptr && _type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0;
}

wxPoint ValueCurvePanel::ToPixel(float x, float y) const {
    wxSize size = GetSize();
    float w = size.GetWidth() - (_leftMargin + X_VC_RIGHT);
    float h = size.GetHeight() - 2 * Y_VC_MARGIN;
    return wxPoint(std::lround(x * w + _leftMargin), std::lround(size.GetHeight() - ToView(y) * h - Y_VC_MARGIN));
}

// The selected point's Bezier handles (normalised), when it is a custom point: the first point has only an out
// handle, the last only an in one.
bool ValueCurvePanel::SelectedHandles(float& px, float& py, bool& hasIn, float& inX, float& inY, bool& hasOut, float& outX, float& outY) const {
    hasIn = hasOut = false;
    if (!IsEditable() || _grabbedPoint < 0) {
        return false;
    }
    const std::list<vcSortablePoint> pts = _vc->GetPoints();
    const float gx = vcSortablePoint::Normalise(_grabbedPoint);
    for (auto it = pts.begin(); it != pts.end(); ++it) {
        if (!(*it == gx)) {
            continue;
        }
        if (it->shape != vcSortablePoint::Shape::Custom) {
            return false;
        }
        px = it->x;
        py = it->y;
        float c1x, c1y, c2x, c2y;
        if (it != pts.begin()) {
            ValueCurve::SegmentControls(*std::prev(it), *it, c1x, c1y, inX, inY);
            hasIn = true;
        }
        if (std::next(it) != pts.end()) {
            ValueCurve::SegmentControls(*it, *std::next(it), outX, outY, c2x, c2y);
            hasOut = true;
        }
        return true;
    }
    return false;
}

// -1 for the selected point's in handle under the mouse, 1 for its out handle, 0 for neither.
int ValueCurvePanel::HandleAt(int mx, int my) const {
    float px, py, inX, inY, outX, outY;
    bool hasIn, hasOut;
    if (!SelectedHandles(px, py, hasIn, inX, inY, hasOut, outX, outY)) {
        return 0;
    }
    auto isNear = [&](float x, float y) {
        wxPoint p = ToPixel(x, y);
        return std::abs(p.x - mx) <= 6 && std::abs(p.y - my) <= 6;
    };
    if (hasOut && isNear(outX, outY)) {
        return 1;
    }
    if (hasIn && isNear(inX, inY)) {
        return -1;
    }
    return 0;
}

// Moves the handle being dragged to (x, y), keeping it on its own side of the point and inside the segment. With
// `mirror` (a modifier held) on a point with both handles, the other handle goes exactly opposite, and the dragged
// one is held to the shorter of the two segments so neither passes a neighbour.
void ValueCurvePanel::DragHandle(float x, float y, bool mirror) {
    const std::list<vcSortablePoint> pts = _vc->GetPoints();
    const float gx = vcSortablePoint::Normalise(_grabbedPoint);
    for (auto it = pts.begin(); it != pts.end(); ++it) {
        if (!(*it == gx)) {
            continue;
        }
        const bool hasPrev = it != pts.begin();
        const bool hasNext = std::next(it) != pts.end();
        const float before = hasPrev ? it->x - std::prev(it)->x : 0.0f; // room on each side
        const float after = hasNext ? std::next(it)->x - it->x : 0.0f;
        mirror = mirror && hasPrev && hasNext;
        float reach = _dragHandle < 0 ? before : after;
        if (mirror) {
            reach = std::min(before, after);
        }
        float dx = _dragHandle < 0 ? std::clamp(x - it->x, -reach, 0.0f) : std::clamp(x - it->x, 0.0f, reach);
        float dy = y - it->y;

        float inDx = it->inDx, inDy = it->inDy, outDx = it->outDx, outDy = it->outDy;
        if (_dragHandle < 0 && hasPrev) {
            inDx = dx;
            inDy = dy;
            if (mirror) {
                outDx = -dx;
                outDy = -dy;
            }
        } else if (_dragHandle > 0 && hasNext) {
            outDx = dx;
            outDy = dy;
            if (mirror) {
                inDx = -dx;
                inDy = -dy;
            }
        }
        _vc->SetPointHandles(it->x, inDx, inDy, outDx, outDy);
        return;
    }
}

void ValueCurvePanel::DrawHandles(wxAutoBufferedPaintDC& pdc) {
    float px, py, inX, inY, outX, outY;
    bool hasIn, hasOut;
    if (!SelectedHandles(px, py, hasIn, inX, inY, hasOut, outX, outY)) {
        return;
    }
    const wxColour orange(255, 160, 40);
    const wxPoint p = ToPixel(px, py);
    pdc.SetBrush(wxBrush(orange));
    for (int side : { -1, 1 }) {
        if ((side < 0 && !hasIn) || (side > 0 && !hasOut)) {
            continue;
        }
        const wxPoint hp = side < 0 ? ToPixel(inX, inY) : ToPixel(outX, outY);
        pdc.SetPen(wxPen(orange, 1, wxPENSTYLE_SOLID));
        pdc.DrawLine(p, hp);
        pdc.SetPen(wxPen(_dragHandle == side ? *wxWHITE : orange, 1, wxPENSTYLE_SOLID));
        pdc.DrawCircle(hp, 4);
    }
    pdc.SetBrush(*wxTRANSPARENT_BRUSH);
}

// Type in a point's position (0-100% across the curve; the ends stay put) and value.
void ValueCurvePanel::EditPoint(float point) {
    const vcSortablePoint* pt = _vc->FindPoint(point);
    if (pt == nullptr) {
        return;
    }
    const vcSortablePoint before = *pt;
    const bool end = point <= 0.0f || point >= 1.0f;

    wxDialog dlg(GetParent(), wxID_ANY, "Edit Point");
    auto* grid = new wxFlexGridSizer(2, 2, 6, 8);
    grid->AddGrowableCol(1);
    auto* xText = new wxTextCtrl(&dlg, wxID_ANY, wxString::Format("%.1f", before.x * 100.0f));
    auto* vText = new wxTextCtrl(&dlg, wxID_ANY, FormatValue(_vc->GetMin() + before.y * (_vc->GetMax() - _vc->GetMin())));
    grid->Add(new wxStaticText(&dlg, wxID_ANY, "X (0 - 100%):"), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(xText, 1, wxEXPAND);
    grid->Add(new wxStaticText(&dlg, wxID_ANY, wxString::Format("Value (%s - %s):", FormatValue(_vc->GetMin()), FormatValue(_vc->GetViewMax()))), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(vText, 1, wxEXPAND);
    if (end) {
        xText->Disable(); // the first and last points are always at 0% and 100%
    }
    auto* top = new wxBoxSizer(wxVERTICAL);
    top->Add(grid, 1, wxALL | wxEXPAND, 10);
    top->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);
    dlg.SetSizerAndFit(top);
    vText->SetFocus();
    vText->SelectAll();
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const float lo = _vc->GetMin() / _vc->GetDivisor();
    const float hi = _vc->GetViewMax() / _vc->GetDivisor();
    const float v = std::clamp((float)std::strtod(vText->GetValue().utf8_str(), nullptr), std::min(lo, hi), std::max(lo, hi));
    float newX = point;
    if (!end) {
        const float step = vcSortablePoint::perPoint();
        const float pct = std::clamp((float)std::strtod(xText->GetValue().utf8_str(), nullptr), 0.0f, 100.0f);
        newX = std::clamp(vcSortablePoint::Normalise(pct / 100.0f), step, 1.0f - step);
        if (newX != point && _vc->IsSetPoint(newX)) {
            wxMessageBox(wxString::Format("There is already a point at %.1f%%.", newX * 100.0f), "Edit Point", wxOK | wxICON_INFORMATION, this);
            return;
        }
    }

    _grabbedPoint = point;
    SaveUndoSelected();
    if (newX != point) {
        _vc->DeletePoint(point);
    }
    _vc->SetValueAt(newX, ToNormalised(v));
    _vc->CopyPointShape(newX, before); // its shape goes with it
    _grabbedPoint = newX;
    NotifyEdited();
    Refresh();
}

// Right-click a point: choose how the curve passes through it.
void ValueCurvePanel::mouseRightDown(wxMouseEvent& event) {
    if (!IsEditable()) {
        return;
    }
    const float px = PointNear(event.GetX(), event.GetY());
    if (px < 0.0f) {
        return;
    }
    const vcSortablePoint* pt = _vc->FindPoint(px);
    if (pt == nullptr) {
        return;
    }
    _grabbedPoint = px;
    NotifySelected();
    Refresh();

    wxMenu menu;
    const int idHard = wxNewId(), idEase = wxNewId(), idCustom = wxNewId();
    menu.AppendRadioItem(idHard, "Hard Point");
    menu.AppendRadioItem(idEase, "Ease");
    menu.AppendRadioItem(idCustom, "Custom");
    menu.Check(pt->shape == vcSortablePoint::Shape::Ease ? idEase : pt->shape == vcSortablePoint::Shape::Custom ? idCustom : idHard, true);
    menu.Bind(wxEVT_MENU, [this, px, idHard, idEase, idCustom](wxCommandEvent& e) {
        vcSortablePoint::Shape shape = e.GetId() == idEase ? vcSortablePoint::Shape::Ease
                                     : e.GetId() == idCustom ? vcSortablePoint::Shape::Custom
                                                             : vcSortablePoint::Shape::Hard;
        _vc->SetPointShape(px, shape);
        _grabbedPoint = px;
        NotifyEdited();
        Refresh();
    });
    PopupMenu(&menu);
}

// Shift, Ctrl or Cmd held: the nearest multiple of 5 of the value as shown, kept inside the curve's range.
float ValueCurvePanel::SnapY(float y, const wxMouseEvent& event) {
    if (!(event.ShiftDown() || event.ControlDown() || event.RawControlDown() || event.CmdDown())) {
        return y;
    }
    const float lo = _vc->GetMin() / _vc->GetDivisor();
    const float hi = _vc->GetViewMax() / _vc->GetDivisor();
    float v = std::round(ToReal(y) / 5.0f) * 5.0f;
    v = std::clamp(v, std::min(lo, hi), std::max(lo, hi));
    return std::clamp(ToNormalised(v), 0.0f, 1.0f);
}

wxString ValueCurvePanel::FormatValue(float raw) const {
    const float v = raw / _vc->GetDivisor();
    switch (_vc->GetDivisor()) {
    case 1:
        // whole numbers as they are, anything else to one decimal
        if (std::fabs(v - std::round(v)) < 0.05f) {
            return wxString::Format("%d", (int)std::lround(v));
        }
        return wxString::Format("%.1f", v);
    case 10:
        return wxString::Format("%.1f", v);
    default:
        return wxString::Format("%.2f", v);
    }
}

// Wide enough for the widest value the margin can show: the five line labels, and a dragged point's value (up to
// one more decimal than the line labels), plus the box drawn around it.
void ValueCurvePanel::UpdateLeftMargin(wxDC& dc) {
    if (_vc == nullptr) {
        return;
    }
    dc.SetFont(GetFont());
    wxCoord widest = 0;
    auto measure = [&](const wxString& s) {
        wxCoord tw = 0, tht = 0;
        dc.GetTextExtent(s, &tw, &tht);
        widest = std::max(widest, tw);
    };
    for (int i = 0; i <= 4; i++) {
        measure(FormatValue(_vc->GetMin() + i * (_vc->GetViewMax() - _vc->GetMin()) / 4.0f));
    }
    for (float raw : { _vc->GetMin(), _vc->GetMax() }) {
        measure(wxString::Format("%.1f", raw / _vc->GetDivisor()));
    }
    _leftMargin = std::max(30.0f, (float)widest + 12.0f);
}

// A grey box around the plot, dotted lines at a quarter, half and three quarters of the range, and the values
// of all five lines down the left.
void ValueCurvePanel::DrawGrid(wxAutoBufferedPaintDC& pdc, float w, float h) {
    const wxColour grey(128, 128, 128);
    const int left = (int)_leftMargin;
    const int top = (int)Y_VC_MARGIN;

    pdc.SetBrush(*wxTRANSPARENT_BRUSH);
    pdc.SetPen(wxPen(grey, 1, wxPENSTYLE_DOT));
    for (int i = 1; i < 4; i++) {
        int y = std::lround(top + h - i * h / 4.0f);
        pdc.DrawLine(left, y, left + (int)w, y);
    }
    pdc.SetPen(wxPen(grey, 1, wxPENSTYLE_SOLID));
    pdc.DrawRectangle(left, top, (int)w + 1, (int)h + 1);

    if (_vc == nullptr) {
        return;
    }
    pdc.SetFont(GetFont());
    pdc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    for (int i = 0; i <= 4; i++) {
        wxString label = FormatValue(_vc->GetMin() + i * (_vc->GetViewMax() - _vc->GetMin()) / 4.0f);
        wxCoord tw = 0, tht = 0;
        pdc.GetTextExtent(label, &tw, &tht);
        int y = std::lround(top + h - i * h / 4.0f);
        pdc.DrawText(label, left - 4 - tw, y - tht / 2);
    }
}

void ValueCurvePanel::Paint(wxPaintEvent& event) {
    // wxPaintDC pdc(this);
    wxAutoBufferedPaintDC pdc(this);
    UpdateLeftMargin(pdc);
    pdc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT)));
    pdc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK)));
    wxSize size = GetSize();
    float w = size.GetWidth() - (_leftMargin + X_VC_RIGHT);
    float th = size.GetHeight();
    float h = size.GetHeight() - 2 * Y_VC_MARGIN;

    pdc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

    DrawGrid(pdc, w, h);
    DrawTiming(pdc);

    pdc.SetBrush(*wxTRANSPARENT_BRUSH);
    if (_vc != nullptr) {
        if (_type == "Custom" && std::lround(_vc->GetParameter1()) > 1) {
            pdc.SetPen(wxPen(*wxRED, 2, wxPENSTYLE_LONG_DASH));
        } else {
            pdc.SetPen(wxPen(*wxGREEN, 2, wxPENSTYLE_LONG_DASH));
        }
        std::list<vcSortablePoint> pts = _vc->GetPoints();
        std::vector<double> offsets;
        int cycles = 1;
        if (_type == "Custom" && std::lround(_vc->GetParameter1()) > 1) {
            cycles = std::lround(_vc->GetParameter1());
            if (cycles < 1) cycles = 1;
            if (cycles > 10) cycles = 10;
            offsets = _vc->GetResampledOffsets(_start, _end, cycles);
        }

        auto alignX = [&offsets, cycles](double val) -> double {
            if (cycles > 1 && !offsets.empty()) {
                int j = (int)(val * cycles);
                if (j >= cycles) j = cycles - 1;
                if (j < 0) j = 0;
                double frac = (val * cycles) - j;
                if (j < (int)offsets.size() - 1) {
                    return offsets[j] + frac * (offsets[j+1] - offsets[j]);
                } else {
                    return 1.0;
                }
            }
            return val;
        };

        // the line through the points, with curved segments drawn as many short pieces
        std::list<vcSortablePoint> line;
        for (auto it = pts.begin(); it != pts.end(); ++it) {
            line.push_back(*it);
            auto nx = std::next(it);
            if (nx != pts.end() && !(it->IsHard() && nx->IsHard()) && it->IsWrapped() == nx->IsWrapped()) {
                constexpr int PIECES = 32;
                for (int s = 1; s < PIECES; ++s) {
                    float sx = it->x + (nx->x - it->x) * s / (float)PIECES;
                    vcSortablePoint sp(sx, std::clamp(ValueCurve::SegmentValue(*it, *nx, sx), 0.0f, 1.0f), it->IsWrapped());
                    sp.x = sx; // off the point grid on purpose
                    line.push_back(sp);
                }
            }
        }
        if (line.size() > 1) {
            std::list<vcSortablePoint>::iterator last = line.begin();
            for (auto p = ++line.begin(); p != line.end(); ++p) {
                double x = alignX(p->x);
                double lastx = alignX(last->x);

                x += (double)_timeOffset / 100.0;
                lastx += (double)_timeOffset / 100.0;

                if (x > 1.0 && lastx <= 1.0) {
                    x -= 1.0;

                    double yat1 = (1.0 - lastx) * (p->y - last->y) + last->y;
                    if (last->IsWrapped() == p->IsWrapped()) {
                        pdc.DrawLine(lastx * w + _leftMargin, th - ToView(last->y) * h - Y_VC_MARGIN,
                                     1.0 * w + _leftMargin, th - ToView(yat1) * h - Y_VC_MARGIN);
                    } else {
                        pdc.DrawLine(lastx * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN,
                                     1.0 * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN);
                    }

                    if (last->IsWrapped() == p->IsWrapped()) {
                        pdc.DrawLine(0.0 * w + _leftMargin, th - ToView(yat1) * h - Y_VC_MARGIN,
                                     x * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN);
                    } else {
                        pdc.DrawLine(0.0 * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN,
                                     x * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN);
                    }
                } else {
                    if (x > 1.0) {
                        x -= 1.0;
                    }
                    if (lastx > 1.0) {
                        lastx -= 1.0;
                    }

                    if (last->IsWrapped() == p->IsWrapped()) {
                        pdc.DrawLine(lastx * w + _leftMargin, th - ToView(last->y) * h - Y_VC_MARGIN,
                                     x * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN);
                    } else {
                        pdc.DrawLine(lastx * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN,
                                     x * w + _leftMargin, th - ToView(p->y) * h - Y_VC_MARGIN);
                    }
                }
                last = p;
            }
        }

        // the points; the selected one is the same square in blue
        const bool selecting = _grabbedPoint != -1 && IsEditable();
        const float selectedX = vcSortablePoint::Normalise(_grabbedPoint);
        if (selecting) {
            DrawHandles(pdc);
        }
        const wxPen red(*wxRED, 2, wxPENSTYLE_SOLID);
        const wxPen blue(*wxBLUE, 2, wxPENSTYLE_SOLID);
        for (auto it = pts.begin(); it != pts.end(); ++it) {
            double x = alignX(it->x);

            x += (double)_timeOffset / 100.0;
            if (x > 1.0) {
                x -= 1.0;
            }
            pdc.SetPen(selecting && *it == selectedX ? blue : red);
            pdc.DrawRectangle((x * w) - 2 + _leftMargin, th - (ToView(it->y) * h) - 2 - Y_VC_MARGIN, 5, 5);
        }

        if (_grabbedPoint != -1 && _type == "Custom" && std::lround(_vc->GetParameter1()) == 1 && _timeOffset == 0) {
            // the point as stored (at its normalised x), not the line a hair beside it at the raw drag position
            const float gx = vcSortablePoint::Normalise(_grabbedPoint);
            const float gy = _vc->IsSetPoint(gx) ? _vc->GetPointAt(gx) : _vc->GetValueAt(_grabbedPoint, 0, 1);
            const int py = std::lround(th - ToView(gy) * h - Y_VC_MARGIN);
            if (HasCapture() && _dragHandle == 0) {
                // dragging: a guide across the plot and the point's value in the left margin, over the other labels
                const wxColour lightBlue(110, 180, 255);
                pdc.SetPen(wxPen(lightBlue, 1, wxPENSTYLE_DOT));
                pdc.DrawLine((int)_leftMargin, py, (int)(_leftMargin + w), py);

                wxString label = FormatValue(_vc->GetMin() + gy * (_vc->GetMax() - _vc->GetMin()));
                pdc.SetFont(GetFont());
                wxCoord tw = 0, tht = 0;
                pdc.GetTextExtent(label, &tw, &tht);
                const int lx = (int)_leftMargin - 4 - tw;
                const int ly = py - tht / 2;
                pdc.SetPen(wxPen(lightBlue, 1, wxPENSTYLE_SOLID));
                pdc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK)));
                pdc.DrawRectangle(lx - 3, ly - 1, tw + 6, tht + 2);
                pdc.SetBrush(*wxTRANSPARENT_BRUSH);
                pdc.SetTextForeground(lightBlue);
                pdc.DrawText(label, lx, ly);
            }
        }
    }
}

void ValueCurveDialog::ValidateWindow() {
    wxString type = Choice1->GetStringSelection();

    float min = 0;
    float max = 100;
    ValueCurve::GetRangeParm1(type.ToStdString(), min, max);
    if (min == MINVOID)
        min = _vc->GetMin();
    if (max == MAXVOID)
        max = _vc->GetViewMax();
    if (Slider_Parameter1->GetMin() != min || Slider_Parameter1->GetMax() != max)
        Slider_Parameter1->SetRange(min, max);
    Slider_Parameter1->SetPageSize(computePageSize(min, max));

    ValueCurve::GetRangeParm2(type.ToStdString(), min, max);
    if (min == MINVOID)
        min = _vc->GetMin();
    if (max == MAXVOID)
        max = _vc->GetViewMax();
    if (Slider_Parameter2->GetMin() != min || Slider_Parameter2->GetMax() != max)
        Slider_Parameter2->SetRange(min, max);
    Slider_Parameter2->SetPageSize(computePageSize(min, max));

    ValueCurve::GetRangeParm3(type.ToStdString(), min, max);
    if (min == MINVOID)
        min = _vc->GetMin();
    if (max == MAXVOID)
        max = _vc->GetViewMax();
    if (Slider_Parameter3->GetMin() != min || Slider_Parameter3->GetMax() != max)
        Slider_Parameter3->SetRange(min, max);
    Slider_Parameter3->SetPageSize(computePageSize(min, max));

    ValueCurve::GetRangeParm4(type.ToStdString(), min, max);
    if (min == MINVOID)
        min = _vc->GetMin();
    if (max == MAXVOID)
        max = _vc->GetViewMax();
    if (Slider_Parameter4->GetMin() != min || Slider_Parameter4->GetMax() != max)
        Slider_Parameter4->SetRange(min, max);
    Slider_Parameter4->SetPageSize(computePageSize(min, max));

    if (type == "Custom") {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Disable();
        TextCtrl_Parameter2->Disable();
        Slider_Parameter3->Disable();
        TextCtrl_Parameter3->Disable();
        Slider_Parameter4->Disable();
        TextCtrl_Parameter4->Disable();
    } else if (type == "Sine" || type == "Abs Sine" || type == "Decaying Sine" || type == "Music Trigger Fade") {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Enable();
        TextCtrl_Parameter2->Enable();
        Slider_Parameter3->Enable();
        TextCtrl_Parameter3->Enable();
        Slider_Parameter4->Enable();
        TextCtrl_Parameter4->Enable();
    } else if (type == "Flat") {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Disable();
        TextCtrl_Parameter2->Disable();
        Slider_Parameter3->Disable();
        TextCtrl_Parameter3->Disable();
        Slider_Parameter4->Disable();
        TextCtrl_Parameter4->Disable();
    } else if (type == "Ramp" || type == "Timing Track Toggle") {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Enable();
        TextCtrl_Parameter2->Enable();
        Slider_Parameter3->Disable();
        TextCtrl_Parameter3->Disable();
        Slider_Parameter4->Disable();
        TextCtrl_Parameter4->Disable();
    } else if (type == "Parabolic Down" || type == "Parabolic Up" || type == "Logarithmic Up" || type == "Logarithmic Down" || type == "Exponential Up" || type == "Exponential Down") {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Enable();
        TextCtrl_Parameter2->Enable();
        Slider_Parameter3->Enable();
        TextCtrl_Parameter3->Enable();
        Slider_Parameter4->Enable();
        TextCtrl_Parameter4->Enable();
    } else if (type == "Saw Tooth" || type == "Ramp Up/Down Hold" || type == "Ramp Up/Down" || type == "Square" || type == "Random" || type == "Music" || type == "Inverted Music" || type == "Timing Track Fade Fixed" || type == "Timing Track Fade Proportional") {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Enable();
        TextCtrl_Parameter2->Enable();
        Slider_Parameter3->Enable();
        TextCtrl_Parameter3->Enable();
        Slider_Parameter4->Disable();
        TextCtrl_Parameter4->Disable();
    } else {
        Slider_Parameter1->Enable();
        TextCtrl_Parameter1->Enable();
        Slider_Parameter2->Enable();
        TextCtrl_Parameter2->Enable();
        Slider_Parameter3->Enable();
        TextCtrl_Parameter3->Enable();
        Slider_Parameter4->Enable();
        TextCtrl_Parameter4->Enable();
    }

    if (type == "Flat") {
        StaticText_P1->SetLabel("Level");
        StaticText_P2->SetLabel("N/A");
        StaticText_P3->SetLabel("N/A");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter2(0);
        _vc->SetParameter3(0);
        _vc->SetParameter4(0);
    } else if (type == "Ramp") {
        StaticText_P1->SetLabel("Start Level");
        StaticText_P2->SetLabel("End Level");
        StaticText_P3->SetLabel("N/A");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter3(0);
        _vc->SetParameter4(0);
    } else if (type == "Timing Track Toggle") {
        StaticText_P1->SetLabel("Low");
        StaticText_P2->SetLabel("High");
        StaticText_P3->SetLabel("N/A");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter3(0);
        _vc->SetParameter4(0);
    } else if (type == "Timing Track Fade Fixed") {
        StaticText_P1->SetLabel("Low");
        StaticText_P2->SetLabel("High");
        StaticText_P3->SetLabel("Frames");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Timing Track Fade Proportional") {
        StaticText_P1->SetLabel("Low");
        StaticText_P2->SetLabel("High");
        StaticText_P3->SetLabel("Prportion");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Music" || type == "Inverted Music") {
        StaticText_P1->SetLabel("Low");
        StaticText_P2->SetLabel("High");
        StaticText_P3->SetLabel("Gain");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Music Trigger Fade") {
        StaticText_P1->SetLabel("Low");
        StaticText_P2->SetLabel("High");
        StaticText_P3->SetLabel("Trigger");
        StaticText_P4->SetLabel("Fade");
    } else if (type == "Ramp Up/Down") {
        StaticText_P1->SetLabel("Start Level");
        StaticText_P2->SetLabel("Mid Level");
        StaticText_P3->SetLabel("End Level");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Ramp Up/Down Hold") {
        StaticText_P1->SetLabel("Start/End Level");
        StaticText_P2->SetLabel("Mid Level");
        StaticText_P3->SetLabel("Mid Level Time");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Saw Tooth") {
        StaticText_P1->SetLabel("Start Level");
        StaticText_P2->SetLabel("End Level");
        StaticText_P3->SetLabel("Cycles");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Parabolic Down") {
        StaticText_P1->SetLabel("Slope");
        StaticText_P2->SetLabel("Vertical Offset");
        StaticText_P3->SetLabel("Start Level");
        StaticText_P4->SetLabel("End Level");
    } else if (type == "Parabolic Up") {
        StaticText_P1->SetLabel("Slope");
        StaticText_P2->SetLabel("Vertical Offset");
        StaticText_P3->SetLabel("Start Level");
        StaticText_P4->SetLabel("End Level");
    } else if (type == "Logarithmic Up") {
        StaticText_P1->SetLabel("Rate");
        StaticText_P2->SetLabel("Vertical Offset");
        StaticText_P3->SetLabel("Start Level");
        StaticText_P4->SetLabel("End Level");
    } else if (type == "Logarithmic Down") {
        StaticText_P1->SetLabel("Rate");
        StaticText_P2->SetLabel("Vertical Offset");
        StaticText_P3->SetLabel("Start Level");
        StaticText_P4->SetLabel("End Level");
    } else if (type == "Exponential Up" || type == "Exponential Down") {
        StaticText_P1->SetLabel("Rate");
        StaticText_P2->SetLabel("Vertical Offset");
        StaticText_P3->SetLabel("Start Level");
        StaticText_P4->SetLabel("End Level");
    } else if (type == "Sine") {
        StaticText_P1->SetLabel("Start");
        StaticText_P2->SetLabel("Amplitude");
        StaticText_P3->SetLabel("Cycles");
        StaticText_P4->SetLabel("Vertical Offset");
    } else if (type == "Decaying Sine") {
        StaticText_P1->SetLabel("Start");
        StaticText_P2->SetLabel("Amplitude");
        StaticText_P3->SetLabel("Cycles");
        StaticText_P4->SetLabel("Vertical Offset");
    } else if (type == "Random") {
        StaticText_P1->SetLabel("Minimum");
        StaticText_P2->SetLabel("Maximum");
        StaticText_P3->SetLabel("Points");
        StaticText_P4->SetLabel("N/A");
    } else if (type == "Abs Sine") {
        StaticText_P1->SetLabel("Start");
        StaticText_P2->SetLabel("Amplitude");
        StaticText_P3->SetLabel("Cycles");
        StaticText_P4->SetLabel("Vertical Offset");
    } else if (type == "Square") {
        StaticText_P1->SetLabel("Start Level");
        StaticText_P2->SetLabel("End Level");
        StaticText_P3->SetLabel("Cycles");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter4(0);
    } else if (type == "Custom") {
        StaticText_P1->SetLabel("Cycles");
        StaticText_P2->SetLabel("N/A");
        StaticText_P3->SetLabel("N/A");
        StaticText_P4->SetLabel("N/A");
        _vc->SetParameter2(0);
        _vc->SetParameter3(0);
        _vc->SetParameter4(0);
    }

    if (type == "Flat" ||
        type == "Saw Tooth" ||
        type == "Ramp Up/Down Hold" ||
        type == "Parabolic Down" ||
        type == "Parabolic Up" ||
        type == "Decaying Sine" ||
        type == "Logarithmic Up" ||
        type == "Logarithmic Down" ||
        type == "Exponential Up" ||
        type == "Random" ||
        type == "Music" ||
        type == "Inverted Music" ||
        type == "Music Trigger Fade" ||
        type == "Timing Track Toggle" ||
        type == "Timing Track Fade Fixed" ||
        type == "Timing Track Fade Proportional" ||
        type == "Exponential Down" ||
        (type == "Custom" && _vc->GetParameter1() > 1.0f)) {
        Button_Reverse->Enable(false);
    } else {
        Button_Reverse->Enable();
    }
    if (type == "Logarithmic Up" ||
        type == "Logarithmic Down" ||
        type == "Sine" ||
        type == "Abs Sine" ||
        type == "Music" ||
        type == "Inverted Music" ||
        type == "Music Trigger Fade" ||
        type == "Timing Track Toggle" ||
        type == "Timing Track Fade Fixed" ||
        type == "Timing Track Fade Proportional" ||
        type == "Decaying Sine" ||
        (type == "Custom" && _vc->GetParameter1() > 1.0f)) {
        Button_Flip->Enable(false);
    } else {
        Button_Flip->Enable();
    }

    if (type == "Timing Track Toggle" ||
        type == "Timing Track Fade Fixed" ||
        type == "Timing Track Fade Proportional") {
        Choice_TimingTrack->Enable();
        TextCtrl_FilterLabel->Enable();
        CheckBox_FilterLabelRegex->Enable();

        if (Choice_TimingTrack->GetStringSelection() == "") {
            Button_Ok->Enable(false);
        } else {
            Button_Ok->Enable();
        }
    } else {
        Button_Ok->Enable();
        Choice_TimingTrack->Enable(false);
        TextCtrl_FilterLabel->Enable(false);
        CheckBox_FilterLabelRegex->Enable(false);
        _vc->SetTimingTrack("");
    }

    // Show audio track selector only for music-reactive types
    bool isMusicType = (type == "Music" || type == "Inverted Music" || type == "Music Trigger Fade");
    bool audioWasVisible = (StaticText_AudioTrack != nullptr && StaticText_AudioTrack->IsShown());
    if (StaticText_AudioTrack != nullptr) StaticText_AudioTrack->Show(isMusicType);
    if (Choice_AudioTrack != nullptr) Choice_AudioTrack->Show(isMusicType);
    if (!isMusicType && _vc != nullptr) {
        _vc->SetAudioTrack("");
    }
    GetSizer()->Layout();
    if (isMusicType != audioWasVisible) {
        int currentWidth = GetSize().x;
        Fit();
        SetSize(currentWidth, GetSize().y);
    }

    UpdateCyclesMode();
}

void ValueCurveDialog::OnChar(wxKeyEvent& event) {
    wxChar uc = event.GetUnicodeKey();
    // the graph decides whether its points are editable (Edit Points edits a one-cycle copy)
    if (_vcp->IsEditable() && (int)uc == (int)WXK_DELETE && _vcp->HasSelected()) {
        _vcp->SaveUndoSelected();
        _vcp->Delete();
    } else if (_vcp->IsEditable() && (uc == 'Z' || uc == 'z') && event.ControlDown() && _vcp->IsDirty()) {
        _vcp->Undo();
    } else {
        event.Skip();
    }
    Refresh();
}

void ValueCurveDialog::OnCheckBox_WrapValuesClick(wxCommandEvent& event) {
    _vc->SetWrap(CheckBox_WrapValues->IsChecked());
    UpdateCyclesMode();
    Refresh();
}

void ValueCurveDialog::OnResize(wxSizeEvent& event) {
    OnSize(event);
    if (PresetSizer->GetChildren().GetCount() > 0) {
        int cols = (PresetSizer->GetSize().GetWidth() - 10) / 32;
        if (cols == 0)
            cols = 1;
        int rows = PresetSizer->GetChildren().GetCount() / cols + 1;
        PresetSizer->SetRows(std::max(1, rows));
        PresetSizer->SetCols(cols);
        PresetSizer->Layout();
    }
    Refresh();
}

void ValueCurveDialog::OnButtonLoadClick(wxCommandEvent& event) {
    std::string id = _vc->GetId(); // save if because it will be overwritten

    wxString filename = wxFileSelector(_("Choose value curve file"), wxEmptyString, wxEmptyString, wxEmptyString, "Value Curve files (*.xvc)|*.xvc", wxFD_OPEN);
    if (filename.IsEmpty())
        return;

    float min = _vc->GetMin();
    float max = _vc->GetMax();
    int div = _vc->GetDivisor();
    _vc->SetLimits(0, 100);
    _vc->SetDivisor(1);
    _vc->LoadXVC(filename.ToStdString());
    _vc->SetLimits(min, max);
    _vc->SetDivisor(div);
    SetSliderMinMax();
    _vc->FixChangedScale(min, max, 1);
    _vcp->Refresh();
    _vcp->ClearUndo();
    _vc->SetId(id);
    Choice1->SetStringSelection(wxString(_vc->GetType().c_str()));
    Slider_TimeOffset->SetValue(_vc->GetTimeOffset());
    CheckBox_WrapValues->SetValue(_vc->GetWrap());
    SetParameter(1, _vc->GetParameter1());
    SetParameter(2, _vc->GetParameter2());
    SetParameter(3, _vc->GetParameter3());
    SetParameter(4, _vc->GetParameter4());
    SetTextCtrlsFromSliders();
    _vcp->SetType(_vc->GetType());
    _vcp->SetTimeOffset(_vc->GetTimeOffset());
    ValidateWindow();
}

void ValueCurveDialog::OnButtonExportClick(wxCommandEvent& event) {
    wxLogNull logNo; // kludge: avoid "error 0" message from wxWidgets after new file is written
    wxString filename = wxFileSelector(_("Choose output file"), wxEmptyString, "ValueCurve", wxEmptyString, "Value Curves (*.xvc)|*.xvc", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (filename.IsEmpty())
        return;

    _vc->SaveXVC(filename.ToStdString());

    _vcp->ClearUndo();

    PopulatePresets();

    _exported = true;
}

void ValueCurveDialog::ProcessPresetDir(wxDir& directory, bool subdirs) {
    
    spdlog::info("ValueCurveDialog Scanning directory for *.xvc files: {}.", (const char*)directory.GetNameWithSep().c_str());

    auto existing = PresetSizer->GetChildren();

    wxArrayString files;
    GetAllFilesInDir(directory.GetNameWithSep(), files, "*.xvc");
    int count = 0;
    for (auto& filename : files) {
        wxFileName fn(filename);
        count++;
        bool found = false;
        for (const auto& it : existing) {
            if (it->GetWindow()->GetLabel() == fn.GetFullPath()) {
                // already there
                found = true;
                break;
            }
        }
        if (!found) {
            ValueCurve vc("");
            vc.LoadXVC(fn.GetFullPath().ToStdString());
            long id = wxNewId();
            wxBitmapButton* bmb = new wxBitmapButton(this, id, GetValueCurveImage(vc, 30, 30, GetContentScaleFactor()), wxDefaultPosition,
                                                     wxSize(30, 30), wxBU_AUTODRAW | wxNO_BORDER);
            bmb->SetLabel(fn.GetFullPath());
            bmb->SetToolTip(fn.GetFullPath());
            PresetSizer->Add(bmb);
            Connect(id, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&ValueCurveDialog::OnButtonPresetClick);
        }
    }
    spdlog::info("    Found {}.", count);

    if (subdirs) {
        wxString filename;
        bool cont = directory.GetFirst(&filename, "*", wxDIR_DIRS);
        while (cont) {
            wxDir dir(directory.GetNameWithSep() + filename);
            ProcessPresetDir(dir, subdirs);
            cont = directory.GetNext(&filename);
        }
    }
}

void ValueCurveDialog::PopulatePresets() {
    
    wxDir dir(xLightsFrame::CurrentDir);

    ProcessPresetDir(dir, false);

    wxString d = ValueCurve::GetValueCurveFolder(xLightsFrame::CurrentDir.ToStdString());

    if (wxDir::Exists(d)) {
        dir.Open(d);
        ProcessPresetDir(dir, true);
    } else {
        spdlog::info("Directory for *.xvc files not found: {}.", (const char*)d.c_str());
    }

    wxStandardPaths stdp = wxStandardPaths::Get();

#ifndef __WXMSW__
    d = wxStandardPaths::Get().GetResourcesDir() + "/valuecurves";
#else
    d = wxFileName(stdp.GetExecutablePath()).GetPath() + "/valuecurves";
#endif
    if (wxDir::Exists(d)) {
        dir.Open(d);
        ProcessPresetDir(dir, true);
    } else {
        spdlog::info("Directory for *.xvc files not found: {}.", (const char*)d.c_str());
    }

    PresetSizer->Layout();
    Layout();
    // Fit();
}

void ValueCurveDialog::OnButtonPresetClick(wxCommandEvent& event) {
    if (_vcp->IsDirty()) {
        if (wxMessageBox("Are you sure you want to discard your current value curve?", "Are you sure?", wxYES_NO | wxCENTER, this) == wxNO) {
            return;
        }
    }

    wxString filename = ((wxBitmapButton*)event.GetEventObject())->GetLabel();

    std::string id = _vc->GetId(); // save if because it will be overwritten
    float min = _vc->GetMin();
    float max = _vc->GetMax();
    int div = _vc->GetDivisor();
    _vc->SetLimits(0, 100);
    _vc->SetDivisor(1);
    _vc->LoadXVC(filename.ToStdString());
    _vc->SetLimits(min, max);
    _vc->SetDivisor(div);
    SetSliderMinMax();
    _vc->FixChangedScale(min, max, 1);
    _vcp->Refresh();
    _vcp->ClearUndo();
    _vc->SetId(id);
    Choice1->SetStringSelection(wxString(_vc->GetType().c_str()));
    Slider_TimeOffset->SetValue(_vc->GetTimeOffset());
    CheckBox_WrapValues->SetValue(_vc->GetWrap());
    SetParameter(1, _vc->GetParameter1());
    SetParameter(2, _vc->GetParameter2());
    SetParameter(3, _vc->GetParameter3());
    SetParameter(4, _vc->GetParameter4());
    SetTextCtrlsFromSliders();
    _vcp->SetType(_vc->GetType());
    _vcp->SetTimeOffset(_vc->GetTimeOffset());
    ValidateWindow();
}

void ValueCurveDialog::OnButton_ReverseClick(wxCommandEvent& event) {
    _vc->Reverse();
    Choice1->SetStringSelection(wxString(_vc->GetType().c_str()));
    Slider_TimeOffset->SetValue(_vc->GetTimeOffset());
    SetParameter(1, _vc->GetParameter1());
    SetParameter(2, _vc->GetParameter2());
    SetParameter(3, _vc->GetParameter3());
    SetParameter(4, _vc->GetParameter4());
    SetTextCtrlsFromSliders();
    _vcp->Refresh();
    _vcp->SetType(_vc->GetType());
    _vcp->SetTimeOffset(_vc->GetTimeOffset());
    _vcp->ClearUndo();
    ValidateWindow();
}

void ValueCurveDialog::OnSlider_TimeOffsetCmdSliderUpdated(wxScrollEvent& event) {
    int i = Slider_TimeOffset->GetValue();
    wxString s = wxString::Format("%d", i);
    if (TextCtrl_TimeOffset->GetValue() != s) {
        TextCtrl_TimeOffset->SetValue(s);
    }
    _vc->SetTimeOffset(i);
    _vcp->SetTimeOffset(i);
    _vcp->Refresh();
}

void ValueCurveDialog::OnTextCtrl_TimeOffsetText(wxCommandEvent& event) {
    int i = wxAtoi(TextCtrl_TimeOffset->GetValue());
    if (Slider_TimeOffset->GetValue() != i) {
        Slider_TimeOffset->SetValue(i);
    }
    _vc->SetTimeOffset(i);
    _vcp->SetTimeOffset(Slider_TimeOffset->GetValue());
    _vcp->Refresh();
}

void ValueCurveDialog::OnButton_FlipClick(wxCommandEvent& event) {
    _vc->Flip();
    Choice1->SetStringSelection(wxString(_vc->GetType().c_str()));
    Slider_TimeOffset->SetValue(_vc->GetTimeOffset());
    SetParameter(1, _vc->GetParameter1());
    SetParameter(2, _vc->GetParameter2());
    SetParameter(3, _vc->GetParameter3());
    SetParameter(4, _vc->GetParameter4());
    SetTextCtrlsFromSliders();
    _vcp->Refresh();
    _vcp->SetType(_vc->GetType());
    _vcp->SetTimeOffset(_vc->GetTimeOffset());
    _vcp->ClearUndo();
    ValidateWindow();
}

void ValueCurveDialog::OnChoice_TimingTrackSelect(wxCommandEvent& event) {
    _vc->SetTimingTrack(Choice_TimingTrack->GetStringSelection());
    ValidateWindow();
}

void ValueCurveDialog::OnTextCtrl_FilterLabelText(wxCommandEvent& event)
{
    _vc->SetFilterLabelText(TextCtrl_FilterLabel->GetValue().ToStdString());
    ValidateWindow();
}

void ValueCurveDialog::OnCheckBox_FilterLabelRegexClick(wxCommandEvent& event)
{
    _vc->SetFilterLabelRegex(CheckBox_FilterLabelRegex->IsChecked());
}

// --- colour keyframe bar ---------------------------------------------------------------------------------------

ValueCurveColorBar::ValueCurveColorBar(wxWindow* parent, ValueCurvePanel* graph) :
    wxWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 26), wxWANTS_CHARS), _graph(graph) {
    SetMinSize(wxSize(200, 26));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetToolTip("Colour over time. Click to add a colour, drag to move it, double-click to change it, right-click for Blend / Hard, Delete to remove it.");
    Bind(wxEVT_PAINT, &ValueCurveColorBar::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &ValueCurveColorBar::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &ValueCurveColorBar::OnLeftUp, this);
    Bind(wxEVT_MOTION, &ValueCurveColorBar::OnMotion, this);
    Bind(wxEVT_LEFT_DCLICK, &ValueCurveColorBar::OnLeftDClick, this);
    Bind(wxEVT_RIGHT_DOWN, &ValueCurveColorBar::OnRightDown, this);
    Bind(wxEVT_KEY_DOWN, &ValueCurveColorBar::OnKeyDown, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ValueCurveColorBar::OnCaptureLost, this);
}

float ValueCurveColorBar::ToX(int mx) const {
    int x0, x1;
    _graph->PlotRange(x0, x1);
    return x1 > x0 ? std::clamp((float)(mx - x0) / (float)(x1 - x0), 0.0f, 1.0f) : 0.0f;
}

int ValueCurveColorBar::ToPixel(float x) const {
    int x0, x1;
    _graph->PlotRange(x0, x1);
    return x0 + (int)std::lround(x * (x1 - x0));
}

// The key under mx (the nearest within a few pixels), or -1.
int ValueCurveColorBar::KeyAt(int mx) const {
    if (_vc == nullptr) {
        return -1;
    }
    const auto& keys = _vc->GetColorKeys();
    int best = -1;
    int bestDist = 8;
    for (int i = 0; i < (int)keys.size(); ++i) {
        const int d = std::abs(ToPixel(keys[i].x) - mx);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void ValueCurveColorBar::Select(int key) {
    _selected = key;
    if (key >= 0 && _onSelected) {
        _onSelected();
    }
    Refresh();
}

void ValueCurveColorBar::SetKeys(std::vector<vcColorKey> keys, const vcColorKey* keep) {
    vcColorKey kept;
    if (keep != nullptr) {
        kept = *keep;
    }
    _vc->SetColorKeys(std::move(keys));
    _selected = -1;
    if (keep != nullptr) {
        const auto& sorted = _vc->GetColorKeys();
        for (int i = 0; i < (int)sorted.size(); ++i) {
            if (sorted[i].x == kept.x && sorted[i].color == kept.color && sorted[i].blend == kept.blend) {
                _selected = i;
                break;
            }
        }
    }
}

void ValueCurveColorBar::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK)));
    dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());
    if (_vc == nullptr) {
        return;
    }
    if (_selected >= (int)_vc->GetColorKeys().size()) {
        _selected = -1;
    }
    int x0, x1;
    _graph->PlotRange(x0, x1);
    const int top = 4, bottom = size.GetHeight() - 4;
    // the colour at every pixel column
    for (int px = x0; px <= x1; ++px) {
        const xlColor c = _vc->GetColorAt(x1 > x0 ? (float)(px - x0) / (float)(x1 - x0) : 0.0f);
        dc.SetPen(wxPen(xlColorToWxColour(c)));
        dc.DrawLine(px, top, px, bottom);
    }
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(wxColour(128, 128, 128)));
    dc.DrawRectangle(x0, top, x1 - x0 + 1, bottom - top);

    // the keys: a circle fades in (Blend), a square cuts in (Hard); the selected one ringed in light blue
    const int cy = size.GetHeight() / 2;
    const auto& keys = _vc->GetColorKeys();
    for (int i = 0; i < (int)keys.size(); ++i) {
        const int cx = ToPixel(keys[i].x);
        const bool sel = i == _selected;
        dc.SetBrush(wxBrush(xlColorToWxColour(keys[i].color)));
        for (int ring = 0; ring < 2; ++ring) {
            // outer ring: light blue when selected, else white; inner: black, so it shows on any colour
            dc.SetPen(ring == 0 ? wxPen(sel ? wxColour(110, 180, 255) : *wxWHITE, sel ? 3 : 2) : wxPen(*wxBLACK, 1));
            const int r = ring == 0 ? 7 : 5;
            if (keys[i].blend) {
                dc.DrawCircle(cx, cy, r);
            } else {
                dc.DrawRectangle(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
            }
        }
    }
}

void ValueCurveColorBar::OnLeftDown(wxMouseEvent& event) {
    if (_vc == nullptr) {
        return;
    }
    SetFocus();
    const int key = KeyAt(event.GetX());
    if (key >= 0) {
        Select(key);
        _dragging = true;
        if (!HasCapture()) {
            CaptureMouse();
        }
        return;
    }
    int x0, x1;
    _graph->PlotRange(x0, x1);
    if (event.GetX() < x0 - 4 || event.GetX() > x1 + 4) {
        return;
    }
    // a new key, starting at the colour already there, then pick its colour
    vcColorKey k;
    k.x = ToX(event.GetX());
    k.color = _vc->GetColorAt(k.x);
    k.blend = true;
    std::vector<vcColorKey> keys = _vc->GetColorKeys();
    keys.push_back(k);
    SetKeys(std::move(keys), &k);
    Select(_selected);
    Changed();
    PickColour(true);
}

void ValueCurveColorBar::OnMotion(wxMouseEvent& event) {
    if (!_dragging || !HasCapture() || _vc == nullptr || _selected < 0) {
        return;
    }
    std::vector<vcColorKey> keys = _vc->GetColorKeys();
    keys[_selected].x = ToX(event.GetX());
    const vcColorKey moved = keys[_selected];
    SetKeys(std::move(keys), &moved);
    Changed();
}

void ValueCurveColorBar::OnLeftUp(wxMouseEvent& event) {
    _dragging = false;
    if (HasCapture()) {
        ReleaseMouse();
    }
}

void ValueCurveColorBar::OnCaptureLost(wxMouseCaptureLostEvent& event) {
    _dragging = false;
}

void ValueCurveColorBar::OnLeftDClick(wxMouseEvent& event) {
    const int key = KeyAt(event.GetX());
    if (key < 0) {
        return;
    }
    _dragging = false;
    if (HasCapture()) {
        ReleaseMouse();
    }
    Select(key);
    PickColour(false);
}

void ValueCurveColorBar::OnRightDown(wxMouseEvent& event) {
    const int key = KeyAt(event.GetX());
    if (key < 0) {
        return;
    }
    Select(key);
    wxMenu menu;
    const int idBlend = wxNewId(), idHard = wxNewId();
    menu.AppendRadioItem(idBlend, "Blend");
    menu.AppendRadioItem(idHard, "Hard");
    menu.Check(_vc->GetColorKeys()[key].blend ? idBlend : idHard, true);
    menu.Bind(wxEVT_MENU, [this, idBlend](wxCommandEvent& e) {
        if (_selected < 0) {
            return;
        }
        std::vector<vcColorKey> keys = _vc->GetColorKeys();
        keys[_selected].blend = e.GetId() == idBlend;
        const vcColorKey changed = keys[_selected];
        SetKeys(std::move(keys), &changed);
        Changed();
    });
    PopupMenu(&menu);
}

void ValueCurveColorBar::OnKeyDown(wxKeyEvent& event) {
    const int code = event.GetKeyCode();
    if ((code == WXK_DELETE || code == WXK_BACK || code == WXK_NUMPAD_DELETE) && _vc != nullptr && _selected >= 0 &&
        _selected < (int)_vc->GetColorKeys().size()) {
        std::vector<vcColorKey> keys = _vc->GetColorKeys();
        keys.erase(keys.begin() + _selected);
        SetKeys(std::move(keys), nullptr);
        Changed();
        return;
    }
    event.Skip();
}

// The colour picker for the selected key; the bar and the render follow every change. Cancel puts things back (and
// takes away a key that was only just added).
void ValueCurveColorBar::PickColour(bool removeOnCancel) {
    if (_vc == nullptr || _selected < 0) {
        return;
    }
    const std::vector<vcColorKey> before = _vc->GetColorKeys();
    vcColorKey added = before[_selected];
    auto apply = [this](const wxColour& c) {
        if (_selected < 0) {
            return;
        }
        std::vector<vcColorKey> keys = _vc->GetColorKeys();
        keys[_selected].color = wxColourToXlColor(c);
        _vc->SetColorKeys(std::move(keys)); // same x, so the order (and the selection) holds
        Changed();
    };
    auto const& [res, colour] = xlColourData::INSTANCE.ShowColorDialog(this, xlColorToWxColour(before[_selected].color), apply);
    if (res == wxID_OK) {
        apply(colour);
    } else {
        std::vector<vcColorKey> keys = before;
        if (removeOnCancel) {
            keys.erase(keys.begin() + _selected);
            SetKeys(std::move(keys), nullptr);
        } else {
            SetKeys(std::move(keys), &added);
        }
        Changed();
    }
}
