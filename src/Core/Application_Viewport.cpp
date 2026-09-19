#include "Application.h"
#include "ApplicationInternal.h"
#include "Render/AssetImporter.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace AppInternal;

/* -------------------------------------------------------------------------
   Viewport window
   ------------------------------------------------------------------------- */

void Application::drawViewportWindow() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = (int)avail.x, h = (int)avail.y;
    if (w > 0 && h > 0 && (w != viewportFbo.width() || h != viewportFbo.height()))
        viewportFbo.resize(w, h);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)viewportFbo.colorTexture(),
                 ImVec2((float)viewportFbo.width(), (float)viewportFbo.height()),
                 ImVec2(0, 1), ImVec2(1, 0));

    viewportImageMin  = origin;
    viewportImageSize = ImVec2((float)viewportFbo.width(), (float)viewportFbo.height());

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_HASH")) {
            if (p->DataSize == sizeof(size_t)) {
                size_t hash = *(const size_t*)p->Data;
                AssetEntry* entry = assets.find(hash);

                if (entry && !entry->loaded) {
                    Mesh m; std::string err;
                    if (AssetImporter::loadMeshInto(entry->fullPath, m, err)) {
                        renderer.setMesh(entry->hash, std::move(m));
                        entry->loaded = true;
                    }
                }

                if (renderer.getMesh(hash)) {
                    ImVec2 mouse = ImGui::GetIO().MousePos;
                    float vpW = viewportImageSize.x;
                    float vpH = viewportImageSize.y;
                    float aspect = vpW / vpH;
                    glm::mat4 vp = camera.projection(aspect) * camera.view();
                    glm::mat4 invVP = glm::inverse(vp);

                    float ndcX = ((mouse.x - viewportImageMin.x) / vpW) * 2.0f - 1.0f;
                    float ndcY = 1.0f - ((mouse.y - viewportImageMin.y) / vpH) * 2.0f;
                    glm::vec4 p0 = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                    glm::vec4 p1 = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
                    glm::vec3 rayOrigin = glm::vec3(p0) / p0.w;
                    glm::vec3 rayDir    = glm::normalize(glm::vec3(p1)/p1.w - rayOrigin);

                    glm::vec3 hit;
                    if (!raycastGround(scene, rayOrigin, rayDir, hit))
                        hit = glm::vec3(0, 0, 0);

                    Instance inst;
                    inst.meshHash = hash;
                    inst.meshName = entry ? entry->name : "asset";
                    inst.position = hit;
                    scene.addInstance(inst);
                    pushToast("Placed " +
                              (entry ? entry->name : std::string("asset")));
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImVec2 mouse = ImGui::GetIO().MousePos;

    bool hovered = ImGui::IsItemHovered();
    {
        bool overImage = (mouse.x >= viewportImageMin.x &&
                          mouse.x <  viewportImageMin.x + viewportImageSize.x &&
                          mouse.y >= viewportImageMin.y &&
                          mouse.y <  viewportImageMin.y + viewportImageSize.y);
        if (overImage && !ImGui::GetIO().WantTextInput) hovered = true;
    }

    if (hovered && !cursorCaptured && !transformActive) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            if (auto* t = tools.active()) t->onMouseWheel(*this, wheel);
        }
    }

    hoveredGatCell.valid = false;
    hoveredHmVertex.valid = false;

    if (hovered && viewportFbo.width() > 0 && viewportFbo.height() > 0) {
        float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
        glm::mat4 vp = camera.projection(aspect) * camera.view();

        float ndcX = ((mouse.x - origin.x) / viewportFbo.width()) * 2.0f - 1.0f;
        float ndcY = 1.0f - ((mouse.y - origin.y) / viewportFbo.height()) * 2.0f;
        glm::mat4 invVP = glm::inverse(vp);
        glm::vec4 p0 = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 p1 = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
        glm::vec3 a = glm::vec3(p0) / p0.w;
        glm::vec3 b = glm::vec3(p1) / p1.w;
        glm::vec3 dir = glm::normalize(b - a);

        if (std::abs(dir.y) > 1e-4f) {
            float tG = (scene.gatOrigin.y - a.y) / dir.y;
            if (tG > 0.0f) {
                glm::vec3 hit = a + dir * tG;
                int cx = (int)std::floor((hit.x - scene.gatOrigin.x) / scene.cellSize);
                int cy = (int)std::floor((hit.z - scene.gatOrigin.z) / scene.cellSize);
                if (scene.inBounds(cx, cy)) {
                    hoveredGatCell.valid = true;
                    hoveredGatCell.x = cx;
                    hoveredGatCell.y = cy;
                }
            }
            float tH = (scene.hmOrigin.y - a.y) / dir.y;
            if (tH > 0.0f) {
                glm::vec3 hit = a + dir * tH;
                int hx = (int)std::round((hit.x - scene.hmOrigin.x) / scene.hmCell);
                int hy = (int)std::round((hit.z - scene.hmOrigin.z) / scene.hmCell);
                if (scene.hmInBounds(hx, hy)) {
                    hoveredHmVertex.valid = true;
                    hoveredHmVertex.x = hx;
                    hoveredHmVertex.y = hy;
                }
            }
        }
    }

    // GIZMO: precompute hover state, then handle input.
    if (gizmoDragAxis != 0) {
        gizmoHoverAxis = gizmoDragAxis;
    } else if (hovered && !cursorCaptured && selectedInstance > 0) {
        gizmoHoverAxis = computeGizmoHitAxis(mouse);
    } else {
        gizmoHoverAxis = 0;
    }

    bool gizmoHandled = false;
    if (gizmoDragAxis != 0 || (hovered && !cursorCaptured)) {
        bool clicked  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool down     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        gizmoHandled = handleGizmoInput(mouse, clicked, down, released);
    }

    if (hovered && !cursorCaptured && !gizmoHandled && !transformActive) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (auto* t = tools.active()) t->onMouseDown(*this, 0, mouse.x, mouse.y);
        }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (auto* t = tools.active()) t->onMouseMove(*this, mouse.x, mouse.y);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (auto* t = tools.active()) t->onMouseUp(*this, 0, mouse.x, mouse.y);
        }
    }

    if (showTextureOverlay) drawTextureOverlayInViewport();
    if (showGatOverlay)     drawGatOverlayInViewport();
    if (selectedInstance > 0) drawGizmoInViewport();

    ImGui::End();
    ImGui::PopStyleVar();
}

/* -------------------------------------------------------------------------
   Gizmo - hit test
   ------------------------------------------------------------------------- */

int Application::computeGizmoHitAxis(const ImVec2& mouse) {
    if (selectedInstance <= 0) return 0;
    Instance* inst = scene.find(selectedInstance);
    if (!inst) return 0;

    float vpW = (float)viewportFbo.width();
    float vpH = (float)viewportFbo.height();
    if (vpW <= 0 || vpH <= 0) return 0;

    float aspect = vpW / vpH;
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    float camDist = glm::length(camera.position - inst->position);
    float len = std::max(1.5f, camDist * 0.12f);

    ImVec2 originPx;
    if (!projectToScreen(vp, viewportImageMin, vpW, vpH,
                         inst->position, originPx)) return 0;

    static const glm::vec3 kAxisDir[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    static const int       kAxisId [3] = { 1, 2, 3 };

    const float kHitPx    = 14.0f;
    const float kMinLenPx = 8.0f;

    int   bestAxis = 0;
    float bestDist = kHitPx;

    for (int i = 0; i < 3; ++i) {
        ImVec2 endPx;
        if (!projectToScreen(vp, viewportImageMin, vpW, vpH,
                             inst->position + kAxisDir[i] * len, endPx))
            continue;

        float dx = endPx.x - originPx.x;
        float dy = endPx.y - originPx.y;
        if (std::sqrt(dx*dx + dy*dy) < kMinLenPx) continue;

        float d = distToSegment2D(mouse, originPx, endPx);
        if (d < bestDist) { bestDist = d; bestAxis = kAxisId[i]; }
    }
    return bestAxis;
}

/* -------------------------------------------------------------------------
   Gizmo - input
   ------------------------------------------------------------------------- */

bool Application::handleGizmoInput(const ImVec2& mouse, bool clicked, bool down, bool released) {
    if (gizmoDragAxis != 0) {
        if (released) {
            if (auto* inst = scene.find(gizmoDragTargetId)) {
                Instance before;
                before.id = gizmoDragTargetId;
                before.position = gizmoDragStartPos;
                before.scale    = gizmoDragStartScale;

                Instance after = *inst;
                bool scaling = gizmoDragScaling;

                struct Cmd : Command {
                    Scene* s; int id; Instance b, a; bool scaling;
                    Cmd(Scene* sc, int id, Instance b, Instance a, bool sc2)
                        : s(sc), id(id), b(b), a(a), scaling(sc2) {}
                    void apply() override { if (auto* i = s->find(id)) *i = a; }
                    void revert() override { if (auto* i = s->find(id)) *i = b; }
                    const char* name() const override {
                        return scaling ? "Gizmo Scale" : "Gizmo Move";
                    }
                };
                commands.push(std::make_unique<Cmd>(
                    &scene, gizmoDragTargetId, before, after, scaling));
            }
            gizmoDragAxis = 0;
            gizmoDragTargetId = -1;
            gizmoDragScaling = false;
            gizmoHoverAxis = 0;
            transformActive = false;
            return true;
        }

        if (down) {
            glm::vec2 d(mouse.x - gizmoDragClickMouse.x,
                        mouse.y - gizmoDragClickMouse.y);
            float projPix = d.x * gizmoDragAxisScreenDir.x
                          + d.y * gizmoDragAxisScreenDir.y;

            Instance* inst = scene.find(gizmoDragTargetId);
            if (inst) {
                if (gizmoDragScaling) {
                    float deltaGizmo = projPix / std::max(1.0f, gizmoDragScreenLen);
                    float factor = 1.0f + deltaGizmo;
                    if (factor < 0.01f) factor = 0.01f;

                    glm::vec3 s = gizmoDragStartScale;
                    if (gizmoDragAxis == 1) s.x *= factor;
                    if (gizmoDragAxis == 2) s.y *= factor;
                    if (gizmoDragAxis == 3) s.z *= factor;
                    inst->scale = s;
                } else {
                    float worldAmt = projPix * (gizmoDragWorldLen /
                                                std::max(1.0f, gizmoDragScreenLen));
                    glm::vec3 newPos = gizmoDragStartPos + gizmoDragAxisDir * worldAmt;

                    if (ImGui::GetIO().KeyCtrl) {
                        float g = gridSnapTranslate;
                        if (g > 1e-5f) {
                            if (gizmoDragAxis == 1) newPos.x = std::round(newPos.x / g) * g;
                            if (gizmoDragAxis == 2) newPos.y = std::round(newPos.y / g) * g;
                            if (gizmoDragAxis == 3) newPos.z = std::round(newPos.z / g) * g;
                        }
                    }
                    inst->position = newPos;
                }
            }
        }
        return true;
    }

    if (!clicked) return false;
    if (gizmoHoverAxis == 0) return false;
    if (transformActive) return false;
    if (selectedInstance <= 0) return false;

    Instance* inst = scene.find(selectedInstance);
    if (!inst) return false;

    float vpW = (float)viewportFbo.width();
    float vpH = (float)viewportFbo.height();
    if (vpW <= 0 || vpH <= 0) return false;

    float aspect = vpW / vpH;
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    float camDist = glm::length(camera.position - inst->position);
    float len = std::max(1.5f, camDist * 0.12f);

    ImVec2 originPx;
    if (!projectToScreen(vp, viewportImageMin, vpW, vpH,
                         inst->position, originPx)) return false;

    int axisIdx = gizmoHoverAxis - 1;
    if (axisIdx < 0 || axisIdx > 2) return false;

    static const glm::vec3 kAxisDir[3] = { {1,0,0}, {0,1,0}, {0,0,1} };

    ImVec2 endPx;
    if (!projectToScreen(vp, viewportImageMin, vpW, vpH,
                         inst->position + kAxisDir[axisIdx] * len, endPx))
        return false;

    glm::vec2 sd(endPx.x - originPx.x, endPx.y - originPx.y);
    float sLen = std::sqrt(sd.x * sd.x + sd.y * sd.y);
    if (sLen < 4.0f) return false;

    gizmoDragAxis     = gizmoHoverAxis;
    gizmoDragScaling  = ImGui::GetIO().KeyShift;
    gizmoDragTargetId = inst->id;
    gizmoDragStartPos = inst->position;
    gizmoDragStartScale = inst->scale;
    gizmoDragClickMouse = { mouse.x, mouse.y };
    gizmoDragAxisDir = kAxisDir[axisIdx];
    gizmoDragAxisScreenDir = sd / sLen;
    gizmoDragScreenLen = sLen;
    gizmoDragWorldLen  = len;

    transformActive = true;
    return true;
}

/* -------------------------------------------------------------------------
   Gizmo - draw
   ------------------------------------------------------------------------- */

void Application::drawGizmoInViewport() {
    Instance* inst = scene.find(selectedInstance);
    if (!inst) return;

    float vpW = (float)viewportFbo.width();
    float vpH = (float)viewportFbo.height();
    if (vpW <= 0 || vpH <= 0) return;

    float aspect = vpW / vpH;
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    float camDist = glm::length(camera.position - inst->position);
    float len = std::max(1.5f, camDist * 0.12f);

    ImVec2 originPx;
    if (!projectToScreen(vp, viewportImageMin, vpW, vpH,
                         inst->position, originPx)) return;

    struct AxisDraw { glm::vec3 dir; ImU32 base; int id; };
    AxisDraw axes[3] = {
        { {1,0,0}, IM_COL32(230, 80, 80, 255), 1 },
        { {0,1,0}, IM_COL32(90, 230, 90, 255), 2 },
        { {0,0,1}, IM_COL32(90, 150, 250, 255), 3 },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (auto& ax : axes) {
        ImVec2 endPx;
        if (!projectToScreen(vp, viewportImageMin, vpW, vpH,
                             inst->position + ax.dir * len, endPx))
            continue;

        bool active = (gizmoDragAxis  == ax.id);
        bool hover  = (gizmoHoverAxis == ax.id);

        float thick = active ? 6.0f : (hover ? 4.5f : 2.5f);

        ImU32 col = ax.base;
        if (active)      col = (col & 0x00FFFFFF) | 0xFF000000;
        else if (hover)  col = (col & 0x00FFFFFF) | 0xF0000000;
        else             col = (col & 0x00FFFFFF) | 0xA0000000;

        dl->AddLine(originPx, endPx, col, thick);

        float dotR = active ? 8.0f : (hover ? 6.0f : 3.0f);
        ImU32 dotCol = active ? IM_COL32(255, 255, 255, 255)
                              : (hover ? IM_COL32(250, 250, 250, 240)
                                       : IM_COL32(220, 220, 220, 160));
        dl->AddCircleFilled(endPx, dotR, dotCol);

        if (hover || active) {
            dl->AddCircle(endPx, dotR + 1.5f,
                          IM_COL32(20, 20, 20, 220), 16, 1.5f);
        }
    }

    dl->AddCircleFilled(originPx, (gizmoDragAxis != 0) ? 6.0f : 4.0f,
                        IM_COL32(255, 255, 255, 230));
    dl->AddCircle(originPx, (gizmoDragAxis != 0) ? 6.0f : 4.0f,
                  IM_COL32(20, 20, 20, 200), 16, 1.5f);

    if (gizmoDragAxis != 0) {
        const char* axName = (gizmoDragAxis == 1) ? "X"
                           : (gizmoDragAxis == 2) ? "Y"
                                                  : "Z";
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      gizmoDragScaling
                        ? "Scaling [%s] - release to commit (Ctrl snap)"
                        : "Moving [%s] - release to commit (Ctrl snap)",
                      axName);
        ImVec2 tp = ImGui::GetIO().MousePos;
        dl->AddText(ImVec2(tp.x + 18, tp.y + 18),
                    IM_COL32(255, 240, 160, 255), buf);
    }
}

/* -------------------------------------------------------------------------
   Overlays
   ------------------------------------------------------------------------- */

void Application::drawGatOverlayInViewport() {
    if (viewportFbo.width() <= 0 || viewportFbo.height() <= 0) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = viewportImageMin;
    float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    auto project = [&](const glm::vec3& world, ImVec2& out) -> bool {
        return projectToScreen(vp, origin,
                               (float)viewportFbo.width(),
                               (float)viewportFbo.height(),
                               world, out);
    };

    for (int y = 0; y < scene.gatH; ++y) {
        for (int x = 0; x < scene.gatW; ++x) {
            GatCell c = scene.at(x, y);
            if (c == GatCell::Walkable) continue;

            glm::vec3 c0 = scene.cellCenter(x, y);
            glm::vec3 e = glm::vec3(scene.cellSize * 0.5f, 0, scene.cellSize * 0.5f);
            ImVec2 p[4];
            if (!project(c0 + glm::vec3(-e.x, 0, -e.z), p[0])) continue;
            if (!project(c0 + glm::vec3( e.x, 0, -e.z), p[1])) continue;
            if (!project(c0 + glm::vec3( e.x, 0,  e.z), p[2])) continue;
            if (!project(c0 + glm::vec3(-e.x, 0,  e.z), p[3])) continue;

            ImU32 col = (c == GatCell::NotWalkable)
                ? IM_COL32(220, 60, 60, 90)
                : IM_COL32(230, 200, 60, 90);

            ImVec2 poly[4] = { p[0], p[1], p[2], p[3] };
            dl->AddConvexPolyFilled(poly, 4, col);
        }
    }

    if (hoveredGatCell.valid) {
        glm::vec3 c0 = scene.cellCenter(hoveredGatCell.x, hoveredGatCell.y);
        glm::vec3 e = glm::vec3(scene.cellSize * 0.5f, 0, scene.cellSize * 0.5f);
        ImVec2 p[4];
        if (project(c0 + glm::vec3(-e.x, 0, -e.z), p[0]) &&
            project(c0 + glm::vec3( e.x, 0, -e.z), p[1]) &&
            project(c0 + glm::vec3( e.x, 0,  e.z), p[2]) &&
            project(c0 + glm::vec3(-e.x, 0,  e.z), p[3])) {
            dl->AddPolyline(p, 4, IM_COL32(255, 255, 255, 220), ImDrawFlags_Closed, 2.0f);
        }
    }
}

void Application::drawTextureOverlayInViewport() {
    if (viewportFbo.width() <= 0 || viewportFbo.height() <= 0) return;

    static const ImU32 kColors[8] = {
        IM_COL32(0,0,0,0),
        IM_COL32(120,180,255,90),
        IM_COL32(255,180,120,90),
        IM_COL32(180,255,120,90),
        IM_COL32(255,120,180,90),
        IM_COL32(120,255,200,90),
        IM_COL32(200,120,255,90),
        IM_COL32(255,255,120,90),
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = viewportImageMin;
    float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    auto project = [&](const glm::vec3& world, ImVec2& out) -> bool {
        return projectToScreen(vp, origin,
                               (float)viewportFbo.width(),
                               (float)viewportFbo.height(),
                               world, out);
    };

    for (int y = 0; y < scene.gatH; ++y) {
        for (int x = 0; x < scene.gatW; ++x) {
            uint8_t L = scene.textureLayers[(size_t)y * scene.gatW + x];
            if (L == 0) continue;
            ImU32 col = kColors[L % 8];
            if ((col & IM_COL32_A_MASK) == 0) continue;

            glm::vec3 c0 = scene.cellCenter(x, y);
            glm::vec3 e = glm::vec3(scene.cellSize * 0.5f, 0, scene.cellSize * 0.5f);
            ImVec2 p[4];
            if (!project(c0 + glm::vec3(-e.x, 0, -e.z), p[0])) continue;
            if (!project(c0 + glm::vec3( e.x, 0, -e.z), p[1])) continue;
            if (!project(c0 + glm::vec3( e.x, 0,  e.z), p[2])) continue;
            if (!project(c0 + glm::vec3(-e.x, 0,  e.z), p[3])) continue;
            ImVec2 poly[4] = { p[0], p[1], p[2], p[3] };
            dl->AddConvexPolyFilled(poly, 4, col);
        }
    }
}