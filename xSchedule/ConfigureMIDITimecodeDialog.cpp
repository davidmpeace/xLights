#include "ConfigureMIDITimecodeDialog.h"

//(*InternalHeaders(ConfigureMIDITimecodeDialog)
#include <wx/intl.h>
#include <wx/string.h>
//*)

#include "events/EventMIDI.h"

//(*IdInit(ConfigureMIDITimecodeDialog)
const long ConfigureMIDITimecodeDialog::ID_STATICTEXT1 = wxNewId();
const long ConfigureMIDITimecodeDialog::ID_CHOICE1 = wxNewId();
const long ConfigureMIDITimecodeDialog::ID_STATICTEXT2 = wxNewId();
const long ConfigureMIDITimecodeDialog::ID_CHOICE2 = wxNewId();
const long ConfigureMIDITimecodeDialog::ID_BUTTON1 = wxNewId();
const long ConfigureMIDITimecodeDialog::ID_BUTTON2 = wxNewId();
//*)

BEGIN_EVENT_TABLE(ConfigureMIDITimecodeDialog,wxDialog)
	//(*EventTable(ConfigureMIDITimecodeDialog)
	//*)
END_EVENT_TABLE()

ConfigureMIDITimecodeDialog::ConfigureMIDITimecodeDialog(wxWindow* parent, std::string midi, int format, wxWindowID id,const wxPoint& pos,const wxSize& size)
{
	//(*Initialize(ConfigureMIDITimecodeDialog)
	wxFlexGridSizer* FlexGridSizer1;
	wxFlexGridSizer* FlexGridSizer2;

	Create(parent, id, _("Configure MIDI Timecode"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE, _T("id"));
	SetClientSize(wxDefaultSize);
	Move(wxDefaultPosition);
	FlexGridSizer1 = new wxFlexGridSizer(0, 2, 0, 0);
	FlexGridSizer1->AddGrowableCol(1);
	StaticText1 = new wxStaticText(this, ID_STATICTEXT1, _("Midi Device:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
	FlexGridSizer1->Add(StaticText1, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	Choice1 = new wxChoice(this, ID_CHOICE1, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE1"));
	FlexGridSizer1->Add(Choice1, 1, wxALL|wxEXPAND, 5);
	StaticText2 = new wxStaticText(this, ID_STATICTEXT2, _("Format:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
	FlexGridSizer1->Add(StaticText2, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	ChoiceFormat = new wxChoice(this, ID_CHOICE2, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE2"));
	ChoiceFormat->SetSelection( ChoiceFormat->Append(_("24 fps")) );
	ChoiceFormat->Append(_("25 fps"));
	ChoiceFormat->Append(_("29.97 fps"));
	ChoiceFormat->Append(_("30 fps"));
	FlexGridSizer1->Add(ChoiceFormat, 1, wxALL|wxEXPAND, 5);
	FlexGridSizer1->Add(-1,-1,1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	FlexGridSizer2 = new wxFlexGridSizer(0, 3, 0, 0);
	Button_Ok = new wxButton(this, ID_BUTTON1, _("Ok"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON1"));
	Button_Ok->SetDefault();
	FlexGridSizer2->Add(Button_Ok, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	ButtonCancel = new wxButton(this, ID_BUTTON2, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_BUTTON2"));
	FlexGridSizer2->Add(ButtonCancel, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	FlexGridSizer1->Add(FlexGridSizer2, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
	SetSizer(FlexGridSizer1);
	FlexGridSizer1->Fit(this);
	FlexGridSizer1->SetSizeHints(this);

	Connect(ID_CHOICE1,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&ConfigureMIDITimecodeDialog::OnChoice1Select);
	Connect(ID_BUTTON1,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ConfigureMIDITimecodeDialog::OnButton_OkClick);
	Connect(ID_BUTTON2,wxEVT_COMMAND_BUTTON_CLICKED,(wxObjectEventFunction)&ConfigureMIDITimecodeDialog::OnButtonCancelClick);
	//*)

    auto devices = EventMIDI::GetDevices();
    for (auto it = devices.begin(); it != devices.end(); ++it)
    {
        Choice1->Append(*it);
    }
    auto outputdevices = EventMIDI::GetOutputDevices();
    for (auto it = outputdevices.begin(); it != outputdevices.end(); ++it)
    {
        Choice1->Append(*it);
    }

    if (midi == "")
    {
        if (Choice1->GetCount() > 0)
        {
            Choice1->SetSelection(0);
        }
    }
    else
    {
        if (!Choice1->SetStringSelection(midi))
        {
            if (Choice1->GetCount() > 0)
            {
                Choice1->SetSelection(0);
            }
        }
    }

    switch(format)
    {
    default:
    case 0:
        ChoiceFormat->SetStringSelection("24 fps");
        break;
    case 1:
        ChoiceFormat->SetStringSelection("25 fps");
        break;
    case 2:
        ChoiceFormat->SetStringSelection("29.97 fps");
        break;
    case 3:
        ChoiceFormat->SetStringSelection("30 fps");
        break;
    }

	ValidateWindow();
}

ConfigureMIDITimecodeDialog::~ConfigureMIDITimecodeDialog()
{
	//(*Destroy(ConfigureMIDITimecodeDialog)
	//*)
}

void ConfigureMIDITimecodeDialog::OnButton_OkClick(wxCommandEvent& event)
{
    EndDialog(wxID_OK);
}

void ConfigureMIDITimecodeDialog::OnButtonCancelClick(wxCommandEvent& event)
{
    EndDialog(wxID_CANCEL);
}

void ConfigureMIDITimecodeDialog::OnChoice1Select(wxCommandEvent& event)
{
    ValidateWindow();
}

void ConfigureMIDITimecodeDialog::ValidateWindow()
{
    if (Choice1->GetStringSelection() != "")
    {
        Button_Ok->Enable();
    }
    else
    {
        Button_Ok->Disable();
    }
}

std::string ConfigureMIDITimecodeDialog::GetMIDI() const
{
    return Choice1->GetStringSelection().ToStdString();
}

int ConfigureMIDITimecodeDialog::GetFormat() const
{
    if (ChoiceFormat->GetStringSelection() == "24 fps")
    {
        return 0;
    }
    if (ChoiceFormat->GetStringSelection() == "25 fps")
    {
        return 1;
    }
    if (ChoiceFormat->GetStringSelection() == "29.97 fps")
    {
        return 2;
    }
    if (ChoiceFormat->GetStringSelection() == "30 fps")
    {
        return 3;
    }
    return 0;
}