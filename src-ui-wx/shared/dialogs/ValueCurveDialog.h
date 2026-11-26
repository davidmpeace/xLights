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

#include <functional>

//(*Headers(ValueCurveDialog)
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

#include "shared/utils/xlCustomControl.h"
#include "render/ValueCurve.h"
#include <wx/dir.h>
#include <wx/timer.h>
#include <wx/tglbtn.h>

#include <memory>
#include <vector>

class Element;
class wxAutoBufferedPaintDC;

class ValueCurvePanel : public wxWindow, public xlCustomControl
{
public:
    ValueCurvePanel(wxWindow* parent, Element* timingElement, int start, int end, wxWindowID id, const wxPoint &pos = wxDefaultPosition,
        const wxSize &size = wxDefaultSize, long style = 0);
    virtual ~ValueCurvePanel() {};
    virtual void SetValue(const std::string &val) override {};
    void SetValue(ValueCurve* vc) { _vc = vc; }
    // Called after the points were edited (moved, added, deleted, reshaped, undone).
    void SetEditedCallback(std::function<void()> cb) { _onEdited = std::move(cb); }
    // Forget the selected point and the undo list (the curve being edited was swapped).
    void ClearSelection() { _grabbedPoint = -1; _dragHandle = 0; _undo.clear(); }
    // Drop the point selection (a colour keyframe was selected instead).
    void Deselect() { if (_grabbedPoint != -1) { _grabbedPoint = -1; Refresh(); } }
    // Called when a point becomes selected.
    void SetSelectedCallback(std::function<void()> cb) { _onSelected = std::move(cb); }
    // The plot's left and right edges in this window's pixels, so the colour bar lines up with it.
    void PlotRange(int& x0, int& x1) const;
    // Points can be edited: a custom curve of one cycle with no time offset.
    [[nodiscard]] bool IsEditable() const;
    void SetType(std::string type) { _type = type; }
    void SetTimeOffset(int timeoffset) { _timeOffset = timeoffset; }
    void Delete();
    void Undo();
    void SaveUndo(float x, float y);
    void SaveUndoSelected();
    void ClearUndo() { _undo.clear(); }
    bool IsDirty() const { return _undo.size() > 0; }
    bool HasSelected() const { return _grabbedPoint != -1; }
    float ToReal(float y);
    float ToNormalised(float y);
    // A raw curve value as the dialog shows it (divided by the curve's divisor).
    wxString FormatValue(float raw) const;

protected:
    DECLARE_EVENT_TABLE()

    void mouseEnter(wxMouseEvent& event);
    void mouseLeave(wxMouseEvent& event);
    void mouseLeftDown(wxMouseEvent& event);
    void mouseLeftUp(wxMouseEvent& event);
    void mouseMoved(wxMouseEvent& event);
    void Paint(wxPaintEvent& event);
    void mouseCaptureLost(wxMouseCaptureLostEvent& event);
    void mouseLeftDClick(wxMouseEvent& event);
    void mouseRightDown(wxMouseEvent& event);

private :
    void Convert(float &x, float &y, wxMouseEvent& event);
    void DrawGrid(wxAutoBufferedPaintDC& pdc, float w, float h);
    float SnapY(float y, const wxMouseEvent& event);
    [[nodiscard]] float ViewScale() const;
    [[nodiscard]] float ToView(float y) const { return y * ViewScale(); }
    [[nodiscard]] float PointNear(int mx, int my) const;
    [[nodiscard]] wxPoint ToPixel(float x, float y) const;
    bool SelectedHandles(float& px, float& py, bool& hasIn, float& inX, float& inY, bool& hasOut, float& outX, float& outY) const;
    [[nodiscard]] int HandleAt(int mx, int my) const;
    void DragHandle(float x, float y, bool mirror);
    void EditPoint(float point);
    void DrawHandles(wxAutoBufferedPaintDC& pdc);
    void UpdateLeftMargin(wxDC& dc);

    void DrawTiming(wxAutoBufferedPaintDC& pdc);
    void DrawTiming(wxAutoBufferedPaintDC& pdc, long timeMS);
    ValueCurve *_vc;
    float _leftMargin = 46.0f;
    int _dragHandle = 0; // the Bezier handle being dragged: -1 in, 1 out, 0 none
    std::function<void()> _onEdited;
    std::function<void()> _onSelected;
    void NotifySelected() { if (_onSelected) _onSelected(); }
    void NotifyEdited() { if (_onEdited) _onEdited(); }
    float _originalGrabbedPoint;
    float _grabbedPoint;
    float _minGrabbedPoint;
    float _maxGrabbedPoint;
    std::string _type;
    int _timeOffset;
    std::list<wxRealPoint> _undo;
    int _start;
    int _end;
    Element* _timingElement;
};

// A value curve's colour keyframes as a gradient bar under the graph, lined up with its plot. Click to add a key
// (and pick its colour), drag to move, double-click to recolour, right-click for Blend / Hard, Delete to remove.
class ValueCurveColorBar : public wxWindow
{
public:
    ValueCurveColorBar(wxWindow* parent, ValueCurvePanel* graph);
    void SetValue(ValueCurve* vc) { _vc = vc; _selected = -1; Refresh(); }
    void Deselect() { if (_selected != -1) { _selected = -1; Refresh(); } }
    // Called when a key becomes selected (so the graph can drop its point selection).
    void SetSelectedCallback(std::function<void()> cb) { _onSelected = std::move(cb); }
    // Called after every change, including each colour while one is being picked, so the render can follow.
    void SetChangedCallback(std::function<void()> cb) { _onChanged = std::move(cb); }

private:
    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnLeftDClick(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnCaptureLost(wxMouseCaptureLostEvent& event);
    [[nodiscard]] int KeyAt(int mx) const;
    [[nodiscard]] float ToX(int mx) const;
    [[nodiscard]] int ToPixel(float x) const;
    void Select(int key);
    // Replaces the keys, keeping `keep` (now wherever sorting put it) selected.
    void SetKeys(std::vector<vcColorKey> keys, const vcColorKey* keep);
    void PickColour(bool removeOnCancel);
    void Changed() { Refresh(); if (_onChanged) _onChanged(); }

    ValueCurve* _vc = nullptr;
    ValueCurvePanel* _graph = nullptr;
    int _selected = -1;
    bool _dragging = false;
    std::function<void()> _onSelected;
    std::function<void()> _onChanged;
};

class ValueCurveDialog: public wxDialog
{
    bool _exported = false;
    // live preview: called when the curve changed and no mouse button is held (see OnLiveTimer)
    std::function<void()> _liveChange;
    wxTimer _liveTimer;
    std::string _liveSig;
    // the graph's max (ValueCurve view max), typed in; up to the real max
    wxSlider* _viewMaxSlider = nullptr;
    wxTextCtrl* _viewMaxText = nullptr;
    void ApplyViewMax();
    void SetViewMax(float raw);
    // Custom curve with Cycles > 1: edit the one cycle of points (on a copy set to one cycle, copied back after every
    // edit) or preview the repeated cycles (read only). Changing Cycles switches to the preview.
    std::unique_ptr<ValueCurve> _editVc;
    bool _cyclesPreview = true;
    std::vector<wxWindow*> _cyclesRow;
    wxToggleButton* _editPointsButton = nullptr;
    wxToggleButton* _cyclesPreviewButton = nullptr;
    void UpdateCyclesMode();
    // the colour keyframe bar under the graph, when the caller allows colour (EnableColor)
    wxBoxSizer* _graphSizer = nullptr;
    ValueCurveColorBar* _colorBar = nullptr;
    // runs the live change callback now (a colour being picked), whatever the mouse is doing
    void PushLiveChange();
    [[nodiscard]] std::string LiveSignature() const;
    void OnLiveTimer(wxTimerEvent& event);
    ValueCurve* _vc;
    ValueCurve _backup;
    bool _slideridd;
    void ValidateWindow();
    void SetParameter(int p, float v);
    void SetParameter100(int p, float v);
    ValueCurvePanel* _vcp = nullptr;
    void UpdateLinkedTextCtrl(wxScrollEvent& event);
    void UpdateLinkedSlider(wxCommandEvent& event);
    void PopulatePresets();
    void ProcessPresetDir(wxDir& directory, bool subdirs);
    void SetTextCtrlsFromSliders();
    void SetSlidersFromTextCtrls();
    void SetTextCtrlFromSlider(int parm, wxTextCtrl* text, int value);
    void SetSliderFromTextCtrl(int parm, wxSlider* slider, float value);
    void SetSliderMinMax();

    public:
    // Re-render as the curve is edited: `onChange` runs after each change, once the mouse is released (so not while
    // a point, handle or slider is being dragged), and again after Cancel puts the original curve back.
    void SetLiveChangeCallback(std::function<void()> onChange);
    // Shows the colour keyframe bar: the curve then carries a colour over time as well as its value.
    void EnableColor();

		ValueCurveDialog(wxWindow* parent, ValueCurve* vc, bool slideridd, SequenceElements* sequenceElements, wxWindowID id=wxID_ANY,const wxPoint& pos=wxDefaultPosition,const wxSize& size=wxDefaultSize);
		virtual ~ValueCurveDialog();
        bool DidExport() const { return _exported; }

		//(*Declarations(ValueCurveDialog)
		wxButton* ButtonExport;
		wxButton* ButtonLoad;
		wxButton* Button_Cancel;
		wxButton* Button_Flip;
		wxButton* Button_Ok;
		wxButton* Button_Reverse;
		wxCheckBox* CheckBox_FilterLabelRegex;
		wxCheckBox* CheckBox_WrapValues;
		wxChoice* Choice1;
		wxChoice* Choice_TimingTrack;
		wxFlexGridSizer* PresetSizer;
		wxSlider* Slider_Parameter1;
		wxSlider* Slider_Parameter2;
		wxSlider* Slider_Parameter3;
		wxSlider* Slider_Parameter4;
		wxSlider* Slider_TimeOffset;
		wxStaticText* StaticText1;
		wxStaticText* StaticText2;
		wxStaticText* StaticText3;
		wxStaticText* StaticText4;
		wxStaticText* StaticText_BottomValue;
		wxStaticText* StaticText_P1;
		wxStaticText* StaticText_P2;
		wxStaticText* StaticText_P3;
		wxStaticText* StaticText_P4;
		wxStaticText* StaticText_TopValue;
		wxTextCtrl* TextCtrl_FilterLabel;
		wxTextCtrl* TextCtrl_Parameter1;
		wxTextCtrl* TextCtrl_Parameter2;
		wxTextCtrl* TextCtrl_Parameter3;
		wxTextCtrl* TextCtrl_Parameter4;
		wxTextCtrl* TextCtrl_TimeOffset;
		//*)

        // Audio track choice (outside wxSmith guard)
        wxStaticText* StaticText_AudioTrack = nullptr;
        wxChoice* Choice_AudioTrack = nullptr;

	protected:

		//(*Identifiers(ValueCurveDialog)
		static const long ID_STATICTEXT3;
		static const long ID_STATICTEXT7;
		static const long ID_STATICTEXT4;
		static const long ID_CHOICE1;
		static const long ID_STATICTEXT1;
		static const long ID_SLIDER_Parameter1;
		static const long IDD_TEXTCTRL_Parameter1;
		static const long ID_STATICTEXT2;
		static const long ID_SLIDER_Parameter2;
		static const long IDD_TEXTCTRL_Parameter2;
		static const long ID_STATICTEXT5;
		static const long ID_SLIDER_Parameter3;
		static const long IDD_TEXTCTRL_Parameter3;
		static const long ID_STATICTEXT6;
		static const long ID_SLIDER_Parameter4;
		static const long IDD_TEXTCTRL_Parameter4;
		static const long ID_CHECKBOX_WrapValues;
		static const long ID_BUTTON5;
		static const long ID_BUTTON6;
		static const long ID_STATICTEXT8;
		static const long ID_SLIDER1;
		static const long ID_TEXTCTRL1;
		static const long ID_STATICTEXT9;
		static const long ID_CHOICE2;
		static const long ID_STATICTEXT10;
		static const long ID_TEXTCTRL2;
		static const long ID_CHECKBOX1;
		static const long ID_BUTTON3;
		static const long ID_BUTTON4;
		static const long ID_BUTTON1;
		static const long ID_BUTTON2;
		//*)

        // Audio track ID (outside wxSmith guard)
        static const long ID_CHOICE_AudioTrack;

	private:

		//(*Handlers(ValueCurveDialog)
		void OnButton_OkClick(wxCommandEvent& event);
		void OnButton_CancelClick(wxCommandEvent& event);
		void OnChoice1Select(wxCommandEvent& event);
		void OnTextCtrl_Parameter1Text(wxCommandEvent& event);
		void OnTextCtrl_Parameter2Text(wxCommandEvent& event);
		void OnSlider_Parameter1CmdSliderUpdated(wxScrollEvent& event);
		void OnSlider_Parameter2CmdSliderUpdated(wxScrollEvent& event);
		void OnSlider_Parameter3CmdSliderUpdated(wxScrollEvent& event);
		void OnTextCtrl_Parameter3Text(wxCommandEvent& event);
		void OnChar(wxKeyEvent& event);
		void OnSlider_Parameter4CmdSliderUpdated(wxScrollEvent& event);
		void OnTextCtrl_Parameter4Text(wxCommandEvent& event);
		void OnCheckBox_WrapValuesClick(wxCommandEvent& event);
		void OnResize(wxSizeEvent& event);
		void OnButtonLoadClick(wxCommandEvent& event);
		void OnButtonExportClick(wxCommandEvent& event);
		void OnButtonPresetClick(wxCommandEvent& event);
		void OnButton_ReverseClick(wxCommandEvent& event);
		void OnSlider_TimeOffsetCmdSliderUpdated(wxScrollEvent& event);
		void OnTextCtrl_TimeOffsetText(wxCommandEvent& event);
		void OnButton_FlipClick(wxCommandEvent& event);
		void OnChoice_TimingTrackSelect(wxCommandEvent& event);
		void OnTextCtrl_FilterLabelText(wxCommandEvent& event);
		void OnCheckBox_FilterLabelRegexClick(wxCommandEvent& event);
		//*)

		DECLARE_EVENT_TABLE()
};
