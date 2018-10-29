#ifndef EFFECTSPANEL_H
#define EFFECTSPANEL_H

//(*Headers(EffectsPanel)
#include <wx/choicebk.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
//*)

class AudioManager;
class SequenceElements;
class Model;
class wxSlider;
class EffectManager;
class xlLockButton;

class EffectsPanel: public wxPanel
{
public:
    bool EffectChanged;
    wxString* CurrentDir;

    EffectsPanel(wxWindow *parent, EffectManager *effects);
    virtual ~EffectsPanel();

    //void SetDefaultPalette();
    void SetDefaultEffectValues(Model *cls, AudioManager* audio, const wxString &name);
    void SetEffectPanelStatus(Model *cls, const wxString &name);
    //void SetButtonColor(wxButton* btn, const wxColour* c);
    void SetSequenceElements(SequenceElements *els);

    wxString GetRandomEffectString(int effidx);
    bool isRandom_();
    bool WantOverlayBkg(); //selectable clear canvas before render -DJ

    //(*Declarations(EffectsPanel)
    wxChoicebook* EffectChoicebook;
    //*)

protected:
    EffectManager *effectManager;

    //(*Identifiers(EffectsPanel)
    static const long ID_CHOICEBOOK1;
    //*)

private:

    //(*Handlers(EffectsPanel)
    void EffectSelected(wxChoicebookEvent& event);
    void OnLockButtonClick(wxCommandEvent& event);
    //*)

    int GetRandomSliderValue(wxSlider* slider);
    wxString GetRandomEffectStringFromWindow(wxWindow *ParentWin, const wxString &prefix);
    wxWindow* GetWindowPanel(wxWindow* w);

    //add lock/unlock/random state flags -DJ
    //these could be used to make fields read-only, but initially they are just used for partially random effects
    bool isRandom_(wxControl* ctl, const char*debug);
#define isRandom(ctl)  isRandom_(ctl, #ctl) //(buttonState[std::string(ctl->GetName())] == Random)
    void setlock(wxButton* button); //, EditState& islocked);

    DECLARE_EVENT_TABLE()
};

#endif
