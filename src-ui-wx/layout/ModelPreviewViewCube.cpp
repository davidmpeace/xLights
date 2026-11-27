/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

// The view cube of the 3D layout: a labelled cube in the top-right corner, turned as the camera is, that turns the
// camera to look from a face, edge or corner when clicked.

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include <wx/wx.h>
#include <wx/graphics.h>
#include <wx/time.h>

#include <glm/gtc/quaternion.hpp>

#include "ModelPreview.h"
#include "shared/utils/wxUtilities.h"
#include "utils/xlImage.h"
#include "xLightsMain.h"

namespace {
constexpr float kPi = 3.14159265f;
constexpr float REGION_EDGE = 0.55f;   // across a face (-1..1), where its edge and corner bands start
constexpr int ANIMATION_MS = 350;
constexpr float BUTTON_HIT = 10.0f;    // px around a button that picks it

const xlColor FACE_COLOUR(222, 226, 232);
const xlColor EDGE_COLOUR(90, 96, 104);
const xlColor TEXT_COLOUR(60, 66, 74);
const xlColor HILITE_COLOUR(110, 170, 255);
const xlColor BUTTON_COLOUR(150, 156, 164);

// The camera's rotation (world to view), as its view matrix applies it: Rx * Ry * Rz.
glm::mat3 CameraRotation(const PreviewCamera& c) {
    glm::mat4 m = glm::rotate(glm::mat4(1.0f), glm::radians(c.GetAngleX()), glm::vec3(1, 0, 0));
    m = m * glm::rotate(glm::mat4(1.0f), glm::radians(c.GetAngleY()), glm::vec3(0, 1, 0));
    m = m * glm::rotate(glm::mat4(1.0f), glm::radians(c.GetAngleZ()), glm::vec3(0, 0, 1));
    return glm::mat3(m);
}

// The angles giving rotation r (Rx * Ry * Rz). glm is column-major: r[col][row].
void SetCameraRotation(PreviewCamera& c, const glm::mat3& r) {
    const float sb = std::clamp(r[2][0], -1.0f, 1.0f);
    const float b = std::asin(sb);
    float a, z;
    if (std::cos(b) > 0.0001f) {
        a = std::atan2(-r[2][1], r[2][2]);
        z = std::atan2(-r[1][0], r[0][0]);
    } else {
        z = 0.0f; // straight up or down: any split of the turn between X and Z will do
        a = std::atan2(r[1][2], r[1][1]);
    }
    c.SetAngleX(glm::degrees(a));
    c.SetAngleY(glm::degrees(b));
    c.SetAngleZ(glm::degrees(z));
}

// The rotation that looks from direction `d` (pivot to camera, world) with `up` up the screen.
glm::mat3 LookFrom(glm::vec3 d, glm::vec3 up) {
    d = glm::normalize(d);
    up = glm::normalize(up - glm::dot(up, d) * d);
    const glm::vec3 right = glm::cross(up, d);
    return glm::transpose(glm::mat3(right, up, d)); // rows: right, up, d
}

// The natural "up" for looking from `d`: world up, or the back / front looking straight down / up.
glm::vec3 NaturalUp(const glm::vec3& d) {
    if (std::abs(d.y) < 0.999f) {
        return glm::vec3(0, 1, 0);
    }
    return d.y > 0 ? glm::vec3(0, 0, -1) : glm::vec3(0, 0, 1);
}

glm::vec3 Row(const glm::mat3& r, int i) {
    return glm::vec3(r[0][i], r[1][i], r[2][i]);
}

// The nearest axis direction to `v`.
glm::vec3 SnapToAxis(const glm::vec3& v) {
    int i = 0;
    for (int k = 1; k < 3; ++k) {
        if (std::abs(v[k]) > std::abs(v[i])) {
            i = k;
        }
    }
    glm::vec3 a(0.0f);
    a[i] = v[i] > 0 ? 1.0f : -1.0f;
    return a;
}

struct Face {
    glm::vec3 n, u, w;            // normal and the two axes across it
    glm::vec3 textRight, textUp;  // how its label reads
    const char* label;
};

const std::array<Face, 6>& Faces() {
    static const std::array<Face, 6> faces = { {
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, "FRONT" },
        { { 0, 0, -1 }, { 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, "BACK" },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, "RIGHT" },
        { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, "LEFT" },
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 0, -1 }, "TOP" },
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, 0, 1 }, "BOTTOM" },
    } };
    return faces;
}

// The span across a face of band -1, 0 or 1.
void Band(int i, float& lo, float& hi) {
    lo = i < 0 ? -1.0f : (i == 0 ? -REGION_EDGE : REGION_EDGE);
    hi = i < 0 ? -REGION_EDGE : (i == 0 ? REGION_EDGE : 1.0f);
}

wxColour Wx(const xlColor& c, uint8_t alpha = 255) {
    return wxColour(c.red, c.green, c.blue, alpha);
}

wxGraphicsPath CubePolygon(wxGraphicsContext* gc, std::initializer_list<glm::vec2> pts) {
    wxGraphicsPath path = gc->CreatePath();
    bool first = true;
    for (const auto& p : pts) {
        if (first) {
            path.MoveToPoint(p.x, p.y);
            first = false;
        } else {
            path.AddLineToPoint(p.x, p.y);
        }
    }
    path.CloseSubpath();
    return path;
}

// A curved arrow round (cx, cy): turning anticlockwise (dir 1) or clockwise (-1) on screen.
void RollArrow(wxGraphicsContext* gc, float cx, float cy, int dir, const xlColor& col) {
    const float r = 7.0f;
    const float a0 = kPi * 0.15f, a1 = kPi * 1.35f;
    wxGraphicsPath arc = gc->CreatePath();
    constexpr int N = 16;
    glm::vec2 end;
    for (int i = 0; i <= N; ++i) {
        const float a = a0 + (a1 - a0) * i / N;
        end = glm::vec2(cx + std::cos(a) * r * dir, cy - std::sin(a) * r);
        if (i == 0) {
            arc.MoveToPoint(end.x, end.y);
        } else {
            arc.AddLineToPoint(end.x, end.y);
        }
    }
    gc->SetPen(wxPen(Wx(col), 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->StrokePath(arc);
    // the head at the end of the arc, along it
    const glm::vec2 tangent = glm::normalize(glm::vec2(-std::sin(a1) * dir, -std::cos(a1)));
    const glm::vec2 normal(-tangent.y, tangent.x);
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(Wx(col)));
    gc->FillPath(CubePolygon(gc, { end + tangent * 4.5f, end - tangent * 2.0f + normal * 4.0f, end - tangent * 2.0f - normal * 4.0f }));
}
}

void ModelPreview::PanCamera3D(float right, float down) {
    // the screen's right and up, in the world: the rows of the camera's rotation
    const glm::vec3 d = glm::transpose(CameraRotation(*camera3d)) * glm::vec3(right, -down, 0.0f);
    SetPan(d.x, d.y, d.z);
}

void ModelPreview::PanCamera3DPixels(float right, float down) {
    // world units per pixel at the depth of the point the camera turns about
    const float depth = std::abs(camera3d->GetDistance() * camera3d->GetZoom());
    const float perPixel = 2.0f * depth * std::tan(glm::radians(22.5f)) / std::max(1.0f, (float)mWindowHeight);
    PanCamera3D(right * perPixel, down * perPixel);
}

bool ModelPreview::ViewCubeShown() const {
    return _viewCubeEnabled && is3d && camera3d != nullptr && mWindowWidth >= 220 && mWindowHeight >= 180;
}

ModelPreview::ViewCubeLayout ModelPreview::GetViewCubeLayout() const {
    return { (float)mWindowWidth - 78.0f, 86.0f, 27.0f };
}

bool ModelPreview::ViewCubeFaceOn() const {
    const glm::vec3 d = Row(CameraRotation(*camera3d), 2);
    return std::max({ std::abs(d.x), std::abs(d.y), std::abs(d.z) }) > std::cos(glm::radians(0.5f));
}

ModelPreview::ViewCubeHit ModelPreview::ViewCubeHitTest(int x, int y) const {
    ViewCubeHit hit;
    if (!ViewCubeShown()) {
        return hit;
    }
    const ViewCubeLayout L = GetViewCubeLayout();
    const glm::vec2 p((float)x, (float)y);
    auto isNear = [&](float bx, float by) { return glm::length(p - glm::vec2(bx, by)) <= BUTTON_HIT; };

    if (isNear(L.cx - 60.0f, L.cy - 62.0f)) {
        hit.part = ViewCubePart::Home;
        return hit;
    }
    const bool animating = _animTo != nullptr;
    if (ViewCubeFaceOn() && !animating) {
        if (isNear(L.cx + 40.0f, L.cy - 62.0f)) {
            hit.part = ViewCubePart::RollLeft;
            return hit;
        }
        if (isNear(L.cx + 62.0f, L.cy - 62.0f)) {
            hit.part = ViewCubePart::RollRight;
            return hit;
        }
        const float a = L.size * 2.1f;
        const std::array<std::pair<ViewCubePart, glm::vec2>, 4> arrows = { {
            { ViewCubePart::ArrowUp, { L.cx, L.cy - a } },
            { ViewCubePart::ArrowDown, { L.cx, L.cy + a } },
            { ViewCubePart::ArrowLeft, { L.cx - a, L.cy } },
            { ViewCubePart::ArrowRight, { L.cx + a, L.cy } },
        } };
        for (const auto& [part, at] : arrows) {
            if (isNear(at.x, at.y)) {
                hit.part = part;
                return hit;
            }
        }
    }

    // the faces the camera sees: where on one, solved from its projected axes
    const glm::mat3 R = CameraRotation(*camera3d);
    auto project = [&](const glm::vec3& v) { glm::vec3 q = R * v; return glm::vec2(q.x, -q.y) * L.size; };
    for (const auto& f : Faces()) {
        if ((R * f.n).z <= 0.01f) {
            continue;
        }
        const glm::vec2 c = glm::vec2(L.cx, L.cy) + project(f.n);
        const glm::vec2 U = project(f.u), W = project(f.w);
        const float det = U.x * W.y - U.y * W.x;
        if (std::abs(det) < 0.0001f) {
            continue;
        }
        const glm::vec2 q = p - c;
        const float alpha = (q.x * W.y - q.y * W.x) / det;
        const float beta = (U.x * q.y - U.y * q.x) / det;
        if (std::abs(alpha) <= 1.0f && std::abs(beta) <= 1.0f) {
            const int iu = alpha < -REGION_EDGE ? -1 : (alpha > REGION_EDGE ? 1 : 0);
            const int iw = beta < -REGION_EDGE ? -1 : (beta > REGION_EDGE ? 1 : 0);
            hit.part = ViewCubePart::Region;
            hit.dir = glm::ivec3(f.n) + iu * glm::ivec3(f.u) + iw * glm::ivec3(f.w);
            return hit;
        }
    }
    return hit;
}

void ModelPreview::AddViewCubeToProgram() {
    if (!ViewCubeShown() || transparentProgram == nullptr) {
        return;
    }
    // the cube's square of the window, drawn at the screen's resolution
    constexpr int REGION = 170;
    const float ox = (float)mWindowWidth - REGION, oy = 0.0f;
    const double scale = std::max(1.0, (double)GetContentScaleFactor());
    const int px = (int)std::lround(REGION * scale);

    const ViewCubeLayout L = GetViewCubeLayout();
    const glm::mat3 R = CameraRotation(*camera3d);
    const ViewCubeHit hover = _viewCubeHover;
    const bool controls = ViewCubeFaceOn() && _animTo == nullptr;

    // redraw the image only when what it shows changes
    std::string key = std::to_string(px) + "|" + std::to_string((int)hover.part) + "," + std::to_string(hover.dir.x) + "," +
                      std::to_string(hover.dir.y) + "," + std::to_string(hover.dir.z) + "|" + (controls ? "c" : "-");
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            key += "|" + std::to_string((int)std::lround(R[c][r] * 2000.0f));
        }
    }
    if (key != _viewCubeKey || _viewCubeImage == nullptr) {
        _viewCubeKey = key;
        wxImage img(px, px);
        img.InitAlpha();
        std::fill(img.GetAlpha(), img.GetAlpha() + px * px, 0);
        std::fill(img.GetData(), img.GetData() + px * px * 3, 0);
        {
            std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(img));
            if (gc) {
                gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
                gc->Scale(scale, scale);
                gc->Translate(-ox, -oy); // window coordinates, as the hit tests use
                auto project = [&](const glm::vec3& v) { glm::vec3 q = R * v; return glm::vec2(L.cx, L.cy) + glm::vec2(q.x, -q.y) * L.size; };

                for (const auto& f : Faces()) {
                    const float facing = (R * f.n).z;
                    if (facing <= 0.01f) {
                        continue;
                    }
                    // the face, shaded by how squarely it faces the camera; the hovered face, edge or corner in blue
                    const float shade = 0.80f + 0.20f * facing;
                    const xlColor face((uint8_t)(FACE_COLOUR.red * shade), (uint8_t)(FACE_COLOUR.green * shade), (uint8_t)(FACE_COLOUR.blue * shade));
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    gc->SetBrush(wxBrush(Wx(face)));
                    gc->FillPath(CubePolygon(gc.get(), { project(f.n - f.u - f.w), project(f.n + f.u - f.w), project(f.n + f.u + f.w), project(f.n - f.u + f.w) }));
                    for (int iu = -1; iu <= 1; ++iu) {
                        for (int iw = -1; iw <= 1; ++iw) {
                            const glm::ivec3 dir = glm::ivec3(f.n) + iu * glm::ivec3(f.u) + iw * glm::ivec3(f.w);
                            if (hover.part != ViewCubePart::Region || hover.dir != dir) {
                                continue;
                            }
                            float u0, u1, w0, w1;
                            Band(iu, u0, u1);
                            Band(iw, w0, w1);
                            gc->SetBrush(wxBrush(Wx(HILITE_COLOUR)));
                            gc->FillPath(CubePolygon(gc.get(), { project(f.n + u0 * f.u + w0 * f.w), project(f.n + u1 * f.u + w0 * f.w),
                                                             project(f.n + u1 * f.u + w1 * f.w), project(f.n + u0 * f.u + w1 * f.w) }));
                        }
                    }

                    // its label, laid exactly on the face (an orthographic face is a parallelogram), fading as it turns away
                    if (facing > 0.15f) {
                        const uint8_t alpha = (uint8_t)(255.0f * std::clamp((facing - 0.15f) / 0.35f, 0.0f, 1.0f));
                        gc->SetFont(wxFont(wxFontInfo(13).Bold()), Wx(TEXT_COLOUR, alpha));
                        wxDouble tw = 0, th = 0;
                        gc->GetTextExtent(f.label, &tw, &th);
                        if (tw > 0 && th > 0) {
                            const glm::vec2 centre = project(f.n);
                            const glm::vec2 right = project(f.n + f.textRight) - centre; // one cube unit across
                            const glm::vec2 up = project(f.n + f.textUp) - centre;
                            const double k = std::min(1.45 / tw, 0.5 / th); // cube units per text pixel
                            const double a = k * right.x, b = k * right.y, c = -k * up.x, d = -k * up.y;
                            gc->PushState();
                            gc->ConcatTransform(gc->CreateMatrix(a, b, c, d, centre.x - (tw / 2) * a - (th / 2) * c, centre.y - (tw / 2) * b - (th / 2) * d));
                            gc->DrawText(f.label, 0, 0);
                            gc->PopState();
                        }
                    }
                    gc->SetPen(wxPen(Wx(EDGE_COLOUR), 1));
                    gc->SetBrush(*wxTRANSPARENT_BRUSH);
                    gc->StrokePath(CubePolygon(gc.get(), { project(f.n - f.u - f.w), project(f.n + f.u - f.w), project(f.n + f.u + f.w), project(f.n - f.u + f.w) }));
                }

                // the house: back to the default viewpoint
                {
                    const xlColor col = hover.part == ViewCubePart::Home ? HILITE_COLOUR : BUTTON_COLOUR;
                    const float hx = L.cx - 60.0f, hy = L.cy - 62.0f;
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    gc->SetBrush(wxBrush(Wx(col)));
                    gc->FillPath(CubePolygon(gc.get(), { { hx, hy - 8.0f }, { hx - 8.5f, hy - 0.5f }, { hx + 8.5f, hy - 0.5f } }));
                    gc->FillPath(CubePolygon(gc.get(), { { hx - 5.5f, hy - 1.0f }, { hx + 5.5f, hy - 1.0f }, { hx + 5.5f, hy + 7.0f }, { hx - 5.5f, hy + 7.0f } }));
                }
                if (controls) {
                    // turn the view 90 degrees
                    RollArrow(gc.get(), L.cx + 40.0f, L.cy - 62.0f, 1, hover.part == ViewCubePart::RollLeft ? HILITE_COLOUR : BUTTON_COLOUR);
                    RollArrow(gc.get(), L.cx + 62.0f, L.cy - 62.0f, -1, hover.part == ViewCubePart::RollRight ? HILITE_COLOUR : BUTTON_COLOUR);
                    // to the face beyond each side
                    const float reach = L.size * 2.1f, t = 6.0f;
                    auto arrow = [&](ViewCubePart part, glm::vec2 at, glm::vec2 out) {
                        const glm::vec2 side(-out.y, out.x);
                        gc->SetPen(*wxTRANSPARENT_PEN);
                        gc->SetBrush(wxBrush(Wx(hover.part == part ? HILITE_COLOUR : BUTTON_COLOUR)));
                        gc->FillPath(CubePolygon(gc.get(), { at + out * t, at - out * t * 0.6f + side * t, at - out * t * 0.6f - side * t }));
                    };
                    arrow(ViewCubePart::ArrowUp, { L.cx, L.cy - reach }, { 0.0f, -1.0f });
                    arrow(ViewCubePart::ArrowDown, { L.cx, L.cy + reach }, { 0.0f, 1.0f });
                    arrow(ViewCubePart::ArrowLeft, { L.cx - reach, L.cy }, { -1.0f, 0.0f });
                    arrow(ViewCubePart::ArrowRight, { L.cx + reach, L.cy }, { 1.0f, 0.0f });
                }
            }
        } // the context writes into the image as it goes
        _viewCubeImage = std::make_shared<xlImage>(wxImageToXlImage(img));
        _viewCubeImageChanged = true;
    }

    // Window pixels to points just in front of the camera: the perspective puts them back on those pixels, in front
    // of the scene. (Undoing the perspective instead loses depth precision with its 1 : 200000 range, and the cube
    // flickers in and out.)
    constexpr float Z = 4.0f; // in view space; well past the near plane (1) on Metal and OpenGL alike
    const float w = (float)mWindowWidth, h = (float)mWindowHeight;
    glm::mat4 toView(0.0f);
    toView[0][0] = 2.0f * Z / (w * ProjMatrix[0][0]);
    toView[3][0] = -Z / ProjMatrix[0][0];
    toView[1][1] = -2.0f * Z / (h * ProjMatrix[1][1]);
    toView[3][1] = Z / ProjMatrix[1][1];
    toView[3][2] = -Z;
    toView[3][3] = 1.0f;
    const glm::mat4 pixels = glm::inverse(ViewMatrix) * toView;

    transparentProgram->addStep([=, this](xlGraphicsContext* ctx) {
        if (_viewCubeImageChanged || _viewCubeTexture == nullptr) {
            delete _viewCubeTexture;
            _viewCubeTexture = ctx->createTexture(*_viewCubeImage, "ViewCube", true);
            _viewCubeImageChanged = false;
        }
        ctx->PushMatrix();
        ctx->ApplyMatrix(pixels); // MVP = P * V * V^-1 * toView = P * toView
        ctx->enableBlending();
        ctx->drawTexture(_viewCubeTexture, ox, oy, ox + REGION, oy + REGION, 0.0f, 0.0f, 1.0f, 1.0f, true);
        ctx->disableBlending();
        ctx->PopMatrix();
    });
}

void ModelPreview::AnimateCameraTo(const PreviewCamera& target) {
    _animFrom = std::make_unique<PreviewCamera>(*camera3d);
    _animTo = std::make_unique<PreviewCamera>(target);
    _animStart = wxGetLocalTimeMillis();
    if (!_viewCubeTimerBound) {
        _viewCubeTimer.SetOwner(this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
            if (_animTo == nullptr) {
                _viewCubeTimer.Stop();
                return;
            }
            float t = std::clamp((float)(wxGetLocalTimeMillis() - _animStart).ToDouble() / ANIMATION_MS, 0.0f, 1.0f);
            const float e = t * t * (3.0f - 2.0f * t); // ease in and out
            // turn along the shortest way round; everything else straight across
            glm::quat q0 = glm::quat_cast(CameraRotation(*_animFrom));
            glm::quat q1 = glm::quat_cast(CameraRotation(*_animTo));
            if (glm::dot(q0, q1) < 0.0f) {
                q1 = -q1;
            }
            SetCameraRotation(*camera3d, glm::mat3_cast(glm::slerp(q0, q1, e)));
            auto mix = [e](float a, float b) { return a + (b - a) * e; };
            camera3d->SetPanX(mix(_animFrom->GetPanX(), _animTo->GetPanX()));
            camera3d->SetPanY(mix(_animFrom->GetPanY(), _animTo->GetPanY()));
            camera3d->SetPanZ(mix(_animFrom->GetPanZ(), _animTo->GetPanZ()));
            camera3d->SetPosX(mix(_animFrom->GetPosX(), _animTo->GetPosX()));
            camera3d->SetPosY(mix(_animFrom->GetPosY(), _animTo->GetPosY()));
            camera3d->SetZoom(mix(_animFrom->GetZoom(), _animTo->GetZoom()));
            camera3d->SetDistance(mix(_animFrom->GetDistance(), _animTo->GetDistance()));
            if (t >= 1.0f) {
                // land exactly on it
                SetCameraRotation(*camera3d, CameraRotation(*_animTo));
                _animTo.reset();
                _animFrom.reset();
                _viewCubeTimer.Stop();
            }
            // mouse dragging turns on from here
            SetCameraPos(0, 0, false, true);
            SetCameraView(0, 0, false, true);
            Refresh(false);
        }, _viewCubeTimer.GetId());
        _viewCubeTimerBound = true;
    }
    _viewCubeTimer.Start(15);
}

void ModelPreview::ViewCubeClicked(const ViewCubeHit& hit) {
    if (hit.part == ViewCubePart::None) {
        return;
    }
    if (hit.part == ViewCubePart::Home) {
        PreviewCamera home(true);
        if (xlights != nullptr && xlights->viewpoint_mgr.GetDefaultCamera3D() != nullptr) {
            home = *xlights->viewpoint_mgr.GetDefaultCamera3D();
        }
        AnimateCameraTo(home);
        return;
    }
    // only the way the camera faces changes: it keeps looking at the same spot, from the same distance
    const glm::mat3 R = CameraRotation(*camera3d);
    glm::mat3 target = R;
    if (hit.part == ViewCubePart::Region) {
        const glm::vec3 d = glm::normalize(glm::vec3(hit.dir));
        target = LookFrom(d, NaturalUp(d));
    } else {
        // face on: square up first so 90 degree turns stay exact
        const glm::vec3 d = SnapToAxis(Row(R, 2));
        const glm::vec3 up = SnapToAxis(Row(R, 1) - glm::dot(Row(R, 1), d) * d);
        const glm::vec3 right = glm::cross(up, d);
        switch (hit.part) {
        case ViewCubePart::RollLeft:
            target = LookFrom(d, right);   // the picture turns anticlockwise
            break;
        case ViewCubePart::RollRight:
            target = LookFrom(d, -right);  // and clockwise
            break;
        case ViewCubePart::ArrowUp:
            target = LookFrom(up, -d);     // look from the face above
            break;
        case ViewCubePart::ArrowDown:
            target = LookFrom(-up, d);
            break;
        case ViewCubePart::ArrowLeft:
            target = LookFrom(-right, up);
            break;
        case ViewCubePart::ArrowRight:
            target = LookFrom(right, up);
            break;
        default:
            break;
        }
    }
    PreviewCamera to(*camera3d);
    SetCameraRotation(to, target);
    AnimateCameraTo(to);
}

bool ModelPreview::ViewCubeMouseDown(wxMouseEvent& event) {
    const ViewCubeHit hit = ViewCubeHitTest(event.GetX(), event.GetY());
    if (hit.part == ViewCubePart::None) {
        return false;
    }
    _viewCubeSwallowUp = true; // its release isn't the layout's either
    ViewCubeClicked(hit);
    return true;
}

bool ModelPreview::ViewCubeMouseMove(wxMouseEvent& event) {
    if (event.Dragging() || !ViewCubeShown()) {
        // a drag started elsewhere goes on as it was
        if (!(_viewCubeHover == ViewCubeHit())) {
            _viewCubeHover = ViewCubeHit();
            Refresh(false);
        }
        return false;
    }
    const ViewCubeHit hit = ViewCubeHitTest(event.GetX(), event.GetY());
    if (!(hit == _viewCubeHover)) {
        _viewCubeHover = hit;
        Refresh(false);
    }
    if (hit.part == ViewCubePart::None) {
        return false;
    }
    static const wxCursor s_hand(wxCURSOR_HAND);
    SetCursor(s_hand);
    return true;
}

bool ModelPreview::ViewCubeMouseUp(wxMouseEvent& event) {
    if (!_viewCubeSwallowUp) {
        return false;
    }
    _viewCubeSwallowUp = false;
    return true;
}

void ModelPreview::ViewCubeMouseLeave() {
    if (!(_viewCubeHover == ViewCubeHit())) {
        _viewCubeHover = ViewCubeHit();
        Refresh(false);
    }
}
