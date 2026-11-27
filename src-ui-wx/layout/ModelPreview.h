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

#include <map>
#include <memory>
#include <set>

#include <wx/wx.h>

#include "graphics/xlGraphicsBase.h"
#include "graphics/xlGraphicsAccumulators.h"
#include "graphics/IModelPreview.h"
#include "utils/xlPoint.h"

#include "Color.h"
#include "render/ViewpointMgr.h"
#include "models/ModelManager.h"
#include "models/ControllerObject.h"

#include "layout/Mouse3DManager.h"

#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// This does not actually fix the pixel size on dialog problem but consolidating this value into one location makes sense
// This is the default size pixels render in things like submodel, faces etc dialogs
#define PIXEL_SIZE_ON_DIALOGS 2

class Model;
class ModelGroup;
class ViewObject;
class PreviewPane;
class LayoutGroup;
class xLightsFrame;
class xlVertex3Accumulator;
class xlImage;

class ModelPreview : public GRAPHICS_BASE_CLASS, public IModelPreview
{
    int _cameraView_last_offsetx = 0;
    int _cameraView_last_offsety = 0;
    int _cameraView_latched_x = 0;
    int _cameraView_latched_y = 0;

    int _cameraPos_last_offsetx = 0;
    int _cameraPos_last_offsety = 0;
    int _cameraPos_latched_x = 0;
    int _cameraPos_latched_y = 0;
    bool _showFirstPixel = false;
    bool _showModelNames = false;
    bool _showModelInfo = false;
    int _labelFontSize = 14; // matches the target height this always rendered at before it became adjustable
    xlColor _labelColor = xlWHITE;
    xlTexture* _fontTexture = nullptr;
    // Keyed by model name, not Model*: the highlight outlives model list
    // rebuilds, and a recycled allocation would otherwise highlight the wrong
    // model.
    std::map<std::string, int> _portStringHighlight;
    std::map<std::string, std::pair<uint32_t, uint32_t>> _portChannelHighlight;
    std::set<std::string> _controllerHighlight;

public:
    ModelPreview(wxPanel* parent, xLightsFrame* xlights = nullptr);
	ModelPreview(wxPanel* parent,
                 xLightsFrame* xlights,
                 bool allowSelected,
                 int style = 0,
                 bool allowPreviewChange = false,
                 bool showFirstPixel = false);
	virtual ~ModelPreview();

    // IModelPreview overrides — resolve ambiguity with GRAPHICS_BASE_CLASS base methods
    int getWidth() const override { return GRAPHICS_BASE_CLASS::getWidth(); }
    int getHeight() const override { return GRAPHICS_BASE_CLASS::getHeight(); }
    std::string getName() const override { return wxWindow::GetName().ToStdString(); }

    // Public Methods
	void InitializePreview(wxString img,int brightness,int alpha, bool center2d0);
    bool StartDrawing(double pointSize, bool fromPaint = false) override;
    void SetPointSize(wxDouble pointSize);
    void Reset();
    // "Keep on Top" for this preview when undocked. Only meaningful once
    // undocked (i.e. wxGetTopLevelParent(this) != xlights); no-ops otherwise.
    bool IsKeptOnTop() const;
    void SetKeptOnTop(bool onTop);
    void EndDrawing(bool swapBuffers=true) override;
	void SetCanvasSize(int width,int height);
    void SetVirtualCanvasSize(int width, int height);
    void GetVirtualCanvasSize(int &w, int& h) const override { w = virtualWidth; h = virtualHeight; }
    int GetVirtualCanvasHeight() const override { return virtualHeight; }
    int GetVirtualCanvasWidth() const override { return virtualWidth; }
	void SetbackgroundImage(wxString image);
    const wxString &GetBackgroundImage() const { return mBackgroundImage;}
	void SetBackgroundBrightness(int brightness, int alpha);
/*
    int GetBackgroundBrightness() const { return mBackgroundBrightness;}
     */
    void SetScaleBackgroundImage(bool b);
    bool GetScaleBackgroundImage() const { return scaleImage; }

	void SetCameraView(int camerax, int cameray, bool latch, bool reset = false);
	void SetCameraPos(int camerax, int cameray, bool latch, bool reset = false);
    void SetZoomDelta(float delta);
    float GetCameraZoomForHandles() const override;
    int GetHandleScale() const override;

    void DrawGroupCentre(float x, float y);
    void SetCenterOffset(ModelGroup* mg, int x, int y);
    void Render();
    void Render(uint32_t frameTime, const unsigned char *data, bool swapBuffers=true);
    void RenderModels(const std::vector<Model*>& models, bool selected, bool showFirstPixel);
    void RenderModel(Model* m, bool wiring = false, bool highlightFirst = false, int highlightpixel = 0);
    // Also the authority for whether a controller box can be picked - see
    // LayoutPanel::IsObjectEditable.
    bool ShouldDrawViewObject(const ViewObject* view_object) const;
    void SetShowModelNames(bool b) { _showModelNames = b; Refresh(); }
    void SetShowModelInfo(bool b) { _showModelInfo = b; Refresh(); }
    // Invalidates the cached glyph texture when the size actually changes --
    // implemented in the .cpp (see there for why this can't just be an inline
    // field assignment).
    void SetLabelFontSize(int size);
    void SetLabelColor(const xlColor& c) { _labelColor = c; Refresh(); }
    int GetLabelFontSize() const { return _labelFontSize; }
    const xlColor& GetLabelColor() const { return _labelColor; }

    double calcPixelSize(double i) override;
    double getViewScale() const override;
    double getBackingScaleFactor() const override;

    void SetModel(const Model* model, bool wiring = false, bool highlightFirst = false);
    void SetActiveLayoutGroup(const std::string &grp = "Default") {
        currentLayoutGroup = grp;
        Refresh();
    }
    const std::vector<Model*> &GetModels();
    ModelGroup* GetSelectedModelGroup();
    bool ValidateModels(const ModelManager& mm);
    bool ValidateModels(const std::vector<Model*>models, const ModelManager& mm);
    void SetAdditionalModel(Model *m) {
        additionalModel = m;
        Refresh();
    }
    Model* GetAdditionalModel() const { return additionalModel; }

    void SetPortStringHighlight(const Model* m, int stringIndex);
    void SetPortChannelHighlight(const Model* m, uint32_t firstChan, uint32_t lastChan);
    void SetControllerHighlights(std::set<std::string> controllerNames) { _controllerHighlight = std::move(controllerNames); }
    void ClearPortStringHighlights() { _portStringHighlight.clear(); _portChannelHighlight.clear(); _controllerHighlight.clear(); }
    bool IsControllerHighlighted(const std::string& controllerName) const override { return _controllerHighlight.count(controllerName) != 0; }

    void SetPreviewPane(PreviewPane* pane) {mPreviewPane = pane;}
    void SetActive(bool show);
    bool GetActive() const;
    float GetZoom() const { return (is3d ? camera3d->GetZoom() : camera2d->GetZoom()); }
    double GetCurrentScaleFactor() const { return std::max((float)virtualWidth / (float)mWindowWidth, (float)virtualHeight / (float)mWindowHeight); }
    float GetCameraRotationX() const override { return (is3d ? camera3d->GetAngleX() : camera2d->GetAngleX()); }
    float GetCameraRotationY() const override { return (is3d ? camera3d->GetAngleY() : camera2d->GetAngleY()); }
    void SetPan(float deltax, float deltay, float deltaz);
    bool IsOrthogonal() const { return camera3d != nullptr && camera3d->IsOrtho(); }
    void SetOrthogonal(bool ortho);
    // pans the 3D camera across the screen as it is turned: by world units, or by pixels so the point turned about follows the mouse
    void PanCamera3D(float right, float down);
    void PanCamera3DPixels(float right, float down);
    struct View2DOffset { float panX, panY, zoomCorrX, zoomCorrY; };
    View2DOffset Get2DViewOffset() const { return { camera2d->GetPanX(), camera2d->GetPanY(), camera2d->GetZoomCorrX(), camera2d->GetZoomCorrY() }; }
    void Set2DViewOffset(const View2DOffset& o) { camera2d->SetPanX(o.panX); camera2d->SetPanY(o.panY); camera2d->SetZoomCorrX(o.zoomCorrX); camera2d->SetZoomCorrY(o.zoomCorrY); }
    void Set3D(bool value) { is3d = value; }
    bool Is3D() const override { return is3d; }
    void SetControllerObjectContext(ControllerObjectContext c) { _controllerObjectContext = c; }
    ControllerObjectContext GetControllerObjectContext() const { return _controllerObjectContext; }
    glm::mat4& GetProjViewMatrix() override { return ProjViewMatrix; }
    glm::mat4& GetProjMatrix() override { return ProjMatrix; }
    glm::mat4& GetViewMatrix() override { return ViewMatrix; }


    void SaveCurrentCameraPosition();
    void SaveDefaultCameraPosition();
    void RestoreDefaultCameraPosition();
    void SetCamera2D(int i);
    void SetCamera3D(int i);
    void SetDisplay2DBoundingBox(bool bb) {
        _display2DBox = bb; 
    }
    void SetDisplay2DGrid(bool grid, long gridSpacing) { _displayGrid = grid; _displayGridSpacing = gridSpacing; grid2dValid=false; }
    void SetDisplay2DCenter0(bool bb) { _center2D0 = bb; grid2dValid=false; }

    bool IsNoCurrentModel() override { return currentModel == "&---none---&"; }
    bool GetShowZoneIndicator() const override;
    bool GetEnablePositionZones() const override;

    void AddBoundingBoxToAccumulator(int x1, int y1, int x2, int y2);
    void AddPathToAccumulator(const std::vector<xlPoint>& path);
    void AddScreenSpaceBoundingBoxToAccumulator(int x1, int y1, int x2, int y2);
    void AddPencilIconToAccumulator();

    static void StartPaintPath(std::vector<xlPoint>& path, int x, int y, bool freeform);
    static void AddPaintPathPoint(std::vector<xlPoint>& path, int x, int y, bool freeform, int minDistanceSq = 4);
    static void EndPaintPath(std::vector<xlPoint>& path, int x, int y, bool freeform);

    static int GetPencilSizeIndex();
    static void SetPencilSizeIndex(int index);
    static void ResetPencilSize();
    // Only previews used for freeform node painting (faces/states/submodels) show the pencil
    void SetSupportsPencil(bool b) { _supportsPencil = b; }
    // Whether the pencil can be used right now (e.g. a submodel is selected and
    // it isn't in SubBuffer mode) -- distinct from _supportsPencil, which just
    // says this preview hosts the feature at all.
    void SetPencilEnabled(bool b) { _pencilEnabled = b; }
    bool IsPencilActive() const;
    float GetPencilCatchRadiusMultiplier() const override;
    std::vector<float> GetPencilStrokeOffsets() const;
    bool HitTestPencilIcon(int x, int y) const;
    void ShowPencilSizeMenu();
    void OnPencilMenuSelected(wxCommandEvent& event);

    // The view cube in the top-right of the 3D layout (as in Fusion 360): a face, edge or corner turns the camera to
    // look from there at what it looks at now; the arrows beside a face-on cube turn to the next face or roll 90 degrees;
    // the house goes to the default viewpoint. Each moves the camera smoothly. The mouse handlers return true for
    // what the cube took (the click, or hovering over it).
    void SetViewCubeEnabled(bool b) { _viewCubeEnabled = b; }
    bool ViewCubeMouseDown(wxMouseEvent& event);
    bool ViewCubeMouseMove(wxMouseEvent& event);
    bool ViewCubeMouseUp(wxMouseEvent& event);
    void ViewCubeMouseLeave();


    void render() override;
    
    bool drawingUsingLogicalSize() const override { return true; }
    virtual bool RequiresDepthBuffer() const override { return true; }

    void setCurrentFrameTime(uint32_t ft) { currentFrameTime = ft; }
    uint32_t getCurrentFrameTime() const override { return currentFrameTime; }
    xlGraphicsContext *getCurrentGraphicsContext() override { return currentContext; }
    xlGraphicsProgram *getCurrentSolidProgram() override { return solidProgram; }
    xlGraphicsProgram *getCurrentTransparentProgram() override { return transparentProgram; }
    
    
    void OnMotion3DEvent(Motion3DEvent &event);
    void OnMotion3DButtonEvent(wxCommandEvent &event);

protected:
    void AddGridToAccumulator(const glm::mat4& ViewScale);

private:
    void setupCameras();

    // view cube (ModelPreviewViewCube.cpp)
    enum class ViewCubePart { None, Region, Home, RollLeft, RollRight, ArrowUp, ArrowDown, ArrowLeft, ArrowRight };
    struct ViewCubeHit {
        ViewCubePart part = ViewCubePart::None;
        glm::ivec3 dir = glm::ivec3(0); // Region: the face, edge or corner, as a direction with -1/0/1 components
        bool operator==(const ViewCubeHit& o) const { return part == o.part && dir == o.dir; }
    };
    struct ViewCubeLayout {
        float cx, cy, size;
    };
    bool ViewCubeShown() const;
    ViewCubeLayout GetViewCubeLayout() const;
    ViewCubeHit ViewCubeHitTest(int x, int y) const;
    bool ViewCubeFaceOn() const;
    void AddViewCubeToProgram();
    void AnimateCameraTo(const PreviewCamera& target);
    void ViewCubeClicked(const ViewCubeHit& hit);
    bool _viewCubeEnabled = false;
    ViewCubeHit _viewCubeHover;
    bool _viewCubeSwallowUp = false;
    wxTimer _viewCubeTimer;
    bool _viewCubeTimerBound = false;
    std::unique_ptr<PreviewCamera> _animFrom;
    std::unique_ptr<PreviewCamera> _animTo;
    wxLongLong _animStart = 0;
    // the cube is drawn (anti-aliased, by wx) into an image, redrawn when what it shows changes
    std::shared_ptr<xlImage> _viewCubeImage;
    std::string _viewCubeKey;
    bool _viewCubeImageChanged = false;
    xlTexture* _viewCubeTexture = nullptr;
	void Paint(wxPaintEvent& event);
	void SetOrigin();
	void mouseMoved(wxMouseEvent& event);
	void mouseLeftDown(wxMouseEvent& event);
	void mouseLeftDClick(wxMouseEvent& event);
	void mouseLeftUp(wxMouseEvent& event);
	void mouseWheelMoved(wxMouseEvent& event);
    void mouseMiddleDown(wxMouseEvent& event);
    void mouseMiddleUp(wxMouseEvent& event);
    //void mouseReleased(wxMouseEvent& event);
	void rightClick(wxMouseEvent& event);
	void mouseLeftWindow(wxMouseEvent& event);
    void mouseCaptureLost(wxMouseCaptureLostEvent& event);

    void OnZoomGesture(wxZoomGestureEvent& event);

    void OnPopup(wxCommandEvent& event);
    void DrawModelNames(const std::vector<Model*>& models);

    bool mIsDrawing = false;
    bool mBackgroundImageExists = false;
    wxString mBackgroundImage;
    int mBackgroundBrightness = 100;
    int mBackgroundAlpha = 100;
    wxDouble mPointSize = 2.0;
    int virtualWidth = 0;
    int virtualHeight = 0;

    bool _display2DBox =  false;
    bool _displayGrid = false;
    long _displayGridSpacing = 100;
    bool _center2D0 = false;
    bool scaleImage = false;
    bool _supportsPencil = false;
    bool _pencilEnabled = true;
    bool allowSelected;
    bool allowPreviewChange;
    ControllerObjectContext _controllerObjectContext = ControllerObjectContext::None;
    PreviewPane* mPreviewPane;

    xLightsFrame* xlights = nullptr;
    std::string currentModel;
    std::string currentLayoutGroup;
    std::vector<std::string> _popupGroupNames; // layout group names in popup menu order
    std::vector<Model*> tmpModelList;
    Model *additionalModel = nullptr;
    static int s_pencilSizeIndex;
    uint32_t currentFrameTime = 0;

    xlGraphicsProgram *solidProgram = nullptr;
    xlGraphicsProgram *transparentProgram = nullptr;
    xlGraphicsProgram *solidViewObjectProgram = nullptr;
    xlGraphicsProgram *transparentViewObjectProgram = nullptr;
    xlTexture *background = nullptr;
    wxSize backgroundSize;
    
    xlVertexAccumulator *grid2d = nullptr;
    bool grid2dValid = false;

    bool _wiring = false;
    bool _highlightFirst = false;
    bool m_mouse_down = false;
    bool m_wheel_down = false;
    int m_last_mouse_x = 0;
    int m_last_mouse_y = 0;
    
    float last_minx = 0.0f;
    float last_miny = 0.0f;
    float last_maxx = 0.0f;
    float last_maxy = 0.0f;

    glm::mat4 ViewMatrix;
    glm::mat4 ProjMatrix;
    glm::mat4 DrawProjMatrix; // 3D: the projection drawn with (ProjMatrix is for picking; they differ only in depth)
    glm::mat4 ProjViewMatrix;

    PreviewCamera* camera3d;
    PreviewCamera* camera2d;
    static const long ID_VIEWPOINT2D;
    static const long ID_VIEWPOINT3D;
    static const long ID_PREVIEW_VIEWPOINT_DEFAULT_RESTORE;
    double currentPixelScaleFactor = 1.0;
    float currentScale2d = 1.0f;


    xlGraphicsContext *currentContext = nullptr;
    std::list<xlTexture *> texturesToDelete;


	DECLARE_EVENT_TABLE()
};
