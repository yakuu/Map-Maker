#include "ToolManager.h"
#include "Core/Application.h"
#include "Core/Shortcuts.h"
#include "Core/Commands.h"
#include "Scene/Scene.h"
#include "Scene/Camera.h"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* =====================================================================
   Select Tool
   ===================================================================== */

class SelectTool : public ITool {
public:
    const char* name() const override { return "Select"; }
    const char* description() const override { return "Click an instance to select it."; }
    const char* statusHint() const override {
        return "LMB pick   |   G/R/S transform   |   F focus   |   Ctrl+D duplicate   |   Shift on Axis to Scale Directional";
    }

    void onMouseDown(Application& app, int button, float mx, float my) override {
        if (button != 0) return;
        int hit = pick(app, mx, my);
        app.selectedInstance = hit;
        if (hit > 0) app.pushToast("Selected instance " + std::to_string(hit));
    }

    void onImGui(Application& app) override {
        if (app.selectedInstance > 0) {
            if (auto* i = app.scene.find(app.selectedInstance)) {
                const Instance before = *i;
                bool changed = false;
                ImGui::Text("ID %d  (%s)", i->id, i->meshName.c_str());
                changed |= ImGui::DragFloat3("Position", &i->position.x, 0.05f);
                changed |= ImGui::DragFloat3("Rotation", &i->rotation.x, 0.5f);
                changed |= ImGui::DragFloat3("Scale", &i->scale.x, 0.01f, 0.001f, 100.0f);
                changed |= ImGui::ColorEdit4("Tint", &i->tint.x);
                if (changed) {
                    app.commands.push(std::make_unique<InstanceStateCommand>(
                        &app.scene, before, *i, "Edit Instance"));
                }
                ImGui::Separator();
                ImGui::TextDisabled("G grab   R rotate   S scale");
                ImGui::TextDisabled("X / Y / Z constrain   |   Ctrl = snap");
                ImGui::TextDisabled("Shift = precise (1/4 speed)");
                ImGui::TextDisabled("LMB/Enter confirm   Esc/RMB cancel");
            } else {
                app.selectedInstance = -1;
            }
        } else {
            ImGui::TextDisabled("Nothing selected.");
        }
    }

private:
    static bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                            const glm::vec3& a, const glm::vec3& b,
                            const glm::vec3& c, float& distance) {
        const glm::vec3 edge1 = b - a;
        const glm::vec3 edge2 = c - a;
        const glm::vec3 p = glm::cross(direction, edge2);
        const float determinant = glm::dot(edge1, p);
        if (std::abs(determinant) < 1e-7f) return false;

        const float invDeterminant = 1.0f / determinant;
        const glm::vec3 fromA = origin - a;
        const float u = glm::dot(fromA, p) * invDeterminant;
        if (u < 0.0f || u > 1.0f) return false;

        const glm::vec3 q = glm::cross(fromA, edge1);
        const float v = glm::dot(direction, q) * invDeterminant;
        if (v < 0.0f || u + v > 1.0f) return false;

        distance = glm::dot(edge2, q) * invDeterminant;
        return distance >= 0.0f;
    }

    static int pick(Application& app, float mx, float my) {
        float vpW = app.viewportImageSize.x;
        float vpH = app.viewportImageSize.y;
        if (vpW <= 0.0f || vpH <= 0.0f) return -1;

        float originX = app.viewportImageMin.x;
        float originY = app.viewportImageMin.y;

        float aspect = vpW / vpH;
        glm::mat4 vp = app.camera.projection(aspect) * app.camera.view();
        const float ndcX = ((mx - originX) / vpW) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((my - originY) / vpH) * 2.0f;
        const glm::mat4 invVp = glm::inverse(vp);
        const glm::vec4 nearClip = invVp * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        const glm::vec4 farClip = invVp * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearClip.w) < 1e-7f || std::abs(farClip.w) < 1e-7f)
            return -1;

        const glm::vec3 rayOrigin = glm::vec3(nearClip) / nearClip.w;
        const glm::vec3 rayDirection =
            glm::normalize(glm::vec3(farClip) / farClip.w - rayOrigin);
        float closestDistance = std::numeric_limits<float>::max();
        int closestId = -1;

        for (const Instance& instance : app.scene.instances) {
            if (std::abs(instance.scale.x) < 1e-7f ||
                std::abs(instance.scale.y) < 1e-7f ||
                std::abs(instance.scale.z) < 1e-7f)
                continue;
            const Mesh* mesh = app.renderer.getMesh(instance.meshHash);
            if (!mesh) continue;
            const auto& vertices = mesh->pickVertices();
            const auto& indices = mesh->pickIndices();
            if (vertices.empty() || indices.size() < 3) continue;

            const glm::mat4 invModel = glm::inverse(Application::modelMatrix(instance));
            const glm::vec4 localOrigin4 = invModel * glm::vec4(rayOrigin, 1.0f);
            if (std::abs(localOrigin4.w) < 1e-7f) continue;
            const glm::vec3 localOrigin = glm::vec3(localOrigin4) / localOrigin4.w;
            const glm::vec3 localDirection =
                glm::vec3(invModel * glm::vec4(rayDirection, 0.0f));

            for (size_t triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
                const uint32_t ia = indices[triangle];
                const uint32_t ib = indices[triangle + 1];
                const uint32_t ic = indices[triangle + 2];
                if (ia >= vertices.size() || ib >= vertices.size() ||
                    ic >= vertices.size())
                    continue;

                float distance = 0.0f;
                if (rayTriangle(localOrigin, localDirection, vertices[ia],
                                vertices[ib], vertices[ic], distance) &&
                    distance < closestDistance) {
                    closestDistance = distance;
                    closestId = instance.id;
                }
            }
        }
        return closestId;
    }
};

/* =====================================================================
   Transform Tool  (Blender-style G / R / S + X/Y/Z + Shift/Ctrl)

   Design notes on snapping:
     - Default = free movement derived from raw mouse delta.
     - Ctrl held = snap the RESULT to the grid, not the delta.
     - Releasing Ctrl immediately returns to free movement.
     - The grid is defined by Application::gridSnap* which the Scene panel
       exposes as sliders.
   ===================================================================== */

class TransformTool : public ITool {
public:
    enum class Op   { None, Grab, Rotate, Scale };
    enum class Axis { None, X, Y, Z };

    const char* name() const override { return "Transform"; }
    const char* description() const override {
        return "Blender-style modal transform on the selected instance.";
    }
    const char* statusHint() const override {
        if (op == Op::None)
            return "Select an object, then G / R / S   |   X/Y/Z axis   |   Ctrl snap   |   Shift precise";

        const char* opName = (op == Op::Grab)   ? "GRAB"
                          : (op == Op::Rotate) ? "ROTATE"
                                               : "SCALE";
        const char* axName = (axis == Axis::X) ? "X"
                          : (axis == Axis::Y) ? "Y"
                          : (axis == Axis::Z) ? "Z"
                                              : "-";
        std::snprintf(activeHint, sizeof(activeHint),
                      "%s [%s]   %s   LMB/Enter confirm   Esc/RMB cancel",
                      opName, axName,
                      snapActive ? "SNAP" : "free");
        return activeHint;
    }

    void onDeactivate(Application& app) override {
        if (op != Op::None) cancel(app);
    }

    void onUpdate(Application& app, float dt) override;
    void onImGui(Application&) override {
        ImGui::TextDisabled("Click to select an object, then:");
        ImGui::BulletText("G  grab (move)");
        ImGui::BulletText("R  rotate");
        ImGui::BulletText("S  scale");
        ImGui::Separator();
        ImGui::TextDisabled("X / Y / Z   constrain axis");
        ImGui::TextDisabled("Ctrl        snap to grid");
        ImGui::TextDisabled("Shift       precise (1/4 speed)");
        ImGui::TextDisabled("LMB/Enter   confirm");
        ImGui::TextDisabled("Esc / RMB   cancel");
    }

private:
    Op    op   = Op::None;
    Axis  axis = Axis::None;
    int   targetId = -1;
    bool  snapActive = false;

    glm::vec3 snapshotPos{0};
    glm::vec3 snapshotRot{0};
    glm::vec3 snapshotScale{1};
    Instance snapshotInstance;

    glm::vec2 startMouse{0,0};
    bool      startMouseDown = false;

    bool edgeG = false, edgeR = false, edgeS = false;
    bool edgeX = false, edgeY = false, edgeZ = false;
    bool edgeEnter = false, edgeEsc = false, edgeLMB = false, edgeRMB = false;

    mutable char activeHint[160] = {0};

    void begin(Application& app, Op newOp);
    void applyDelta(Application& app, glm::vec2 dm, float mul, bool snap);
    void commit(Application& app);
    void cancel(Application& app);
};

void TransformTool::begin(Application& app, Op newOp) {
    if (app.selectedInstance <= 0) return;
    Instance* inst = app.scene.find(app.selectedInstance);
    if (!inst) return;

    op = newOp;
    axis = Axis::None;
    targetId = inst->id;
    snapActive = false;

    snapshotPos   = inst->position;
    snapshotRot   = inst->rotation;
    snapshotScale = inst->scale;
    snapshotInstance = *inst;

    ImVec2 m = ImGui::GetIO().MousePos;
    startMouse = { m.x, m.y };
    startMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    app.transformActive = true;
}

void TransformTool::applyDelta(Application& app, glm::vec2 dm, float mul, bool snap) {
    Instance* inst = app.scene.find(targetId);
    if (!inst) return;

    snapActive = snap;

    switch (op) {
        case Op::Grab: {
            float speed = 0.02f * mul;
            glm::vec3 right = app.camera.right();
            glm::vec3 up    = glm::vec3(0, 1, 0);

            glm::vec3 delta(0.0f);
            if (axis == Axis::X)      delta.x = dm.x * speed;
            else if (axis == Axis::Y) delta.y = -dm.y * speed;
            else if (axis == Axis::Z) delta.z = dm.x * speed;
            else {
                delta += right * (dm.x * speed);
                delta += up    * (-dm.y * speed);
            }

            glm::vec3 newPos = snapshotPos + delta;

            if (snap) {
                float g = app.gridSnapTranslate;
                newPos = glm::round(newPos / g) * g;
                // Re-apply axis constraint after snapping so the other
                // components stay at their snapshot values.
                if (axis == Axis::X) { newPos.y = snapshotPos.y; newPos.z = snapshotPos.z; }
                if (axis == Axis::Y) { newPos.x = snapshotPos.x; newPos.z = snapshotPos.z; }
                if (axis == Axis::Z) { newPos.x = snapshotPos.x; newPos.y = snapshotPos.y; }
            } else if (axis != Axis::None) {
                if (axis == Axis::X) { newPos.y = snapshotPos.y; newPos.z = snapshotPos.z; }
                if (axis == Axis::Y) { newPos.x = snapshotPos.x; newPos.z = snapshotPos.z; }
                if (axis == Axis::Z) { newPos.x = snapshotPos.x; newPos.y = snapshotPos.y; }
            }

            inst->position = newPos;
            app.transformOrigin = newPos;
        } break;

        case Op::Rotate: {
            float speed = 0.5f * mul;
            glm::vec3 rot = snapshotRot;
            float d = dm.x * speed;

            if (axis == Axis::X)      rot.x = snapshotRot.x + d;
            else if (axis == Axis::Y) rot.y = snapshotRot.y + d;
            else if (axis == Axis::Z) rot.z = snapshotRot.z + d;
            else                      rot.y = snapshotRot.y + d;

            if (snap) {
                float g = app.gridSnapRotate;
                rot.x = std::round(rot.x / g) * g;
                rot.y = std::round(rot.y / g) * g;
                rot.z = std::round(rot.z / g) * g;
            }
            inst->rotation = rot;
            app.transformOrigin = inst->position;
        } break;

        case Op::Scale: {
            float speed = 0.01f * mul;
            float factor = 1.0f + dm.x * speed;
            if (factor < 0.01f) factor = 0.01f;

            glm::vec3 s = snapshotScale;
            if (axis == Axis::X)      s.x = snapshotScale.x * factor;
            else if (axis == Axis::Y) s.y = snapshotScale.y * factor;
            else if (axis == Axis::Z) s.z = snapshotScale.z * factor;
            else                      s   = snapshotScale * factor;

            if (snap) {
                float g = app.gridSnapScale;
                s.x = std::round(s.x / g) * g;
                s.y = std::round(s.y / g) * g;
                s.z = std::round(s.z / g) * g;
                s.x = std::max(s.x, g);
                s.y = std::max(s.y, g);
                s.z = std::max(s.z, g);
            }
            inst->scale = s;
            app.transformOrigin = inst->position;
        } break;

        default: break;
    }
}

void TransformTool::commit(Application& app) {
    Instance* inst = app.scene.find(targetId);
    if (inst) {
        Instance after = *inst;
        app.commands.push(std::make_unique<InstanceStateCommand>(
            &app.scene, snapshotInstance, after, "Transform"));
    }
    op = Op::None;
    axis = Axis::None;
    targetId = -1;
    snapActive = false;
    app.transformActive = false;
    app.transformOp = 0;
    app.transformAxis = 0;
}

void TransformTool::cancel(Application& app) {
    Instance* inst = app.scene.find(targetId);
    if (inst) {
        inst->position = snapshotPos;
        inst->rotation = snapshotRot;
        inst->scale    = snapshotScale;
    }
    op = Op::None;
    axis = Axis::None;
    targetId = -1;
    snapActive = false;
    app.transformActive = false;
    app.transformOp = 0;
    app.transformAxis = 0;
}

void TransformTool::onUpdate(Application& app, float) {
    GLFWwindow* w = app.window;
    if (!w) return;

    // Publish the visual state to Application so the viewport overlay can
    // draw the gizmo.
    app.transformOp   = (op == Op::Grab) ? 1 : (op == Op::Rotate) ? 2 : (op == Op::Scale) ? 3 : 0;
    app.transformAxis = (axis == Axis::X) ? 1 : (axis == Axis::Y) ? 2 : (axis == Axis::Z) ? 3 : 0;

    bool g = glfwGetKey(w, GLFW_KEY_G) == GLFW_PRESS;
    bool r = glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS;
    bool s = glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS;
    bool x = glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS;
    bool y = glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS;
    bool z = glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS;
    bool enter = glfwGetKey(w, GLFW_KEY_ENTER) == GLFW_PRESS;
    bool esc   = glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    bool lmb   = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rmb   = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    bool shift = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
              || glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    bool ctrl  = glfwGetKey(w, GLFW_KEY_LEFT_CONTROL)  == GLFW_PRESS
              || glfwGetKey(w, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    bool eG = g && !edgeG, eR = r && !edgeR, eS = s && !edgeS;
    bool eX = x && !edgeX, eY = y && !edgeY, eZ = z && !edgeZ;
    bool eEnter = enter && !edgeEnter;
    bool eEsc   = esc && !edgeEsc;
    bool eLMB   = lmb && !edgeLMB;
    bool eRMB   = rmb && !edgeRMB;
    edgeG=g; edgeR=r; edgeS=s; edgeX=x; edgeY=y; edgeZ=z;
    edgeEnter=enter; edgeEsc=esc; edgeLMB=lmb; edgeRMB=rmb;

    if (op == Op::None) {
        if (app.selectedInstance <= 0) return;
        if (ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard)
            return;
        if (eG) begin(app, Op::Grab);
        else if (eR) begin(app, Op::Rotate);
        else if (eS) begin(app, Op::Scale);
        return;
    }

    if (eEsc || eRMB) { cancel(app); return; }
    if (eEnter || (eLMB && !startMouseDown)) { commit(app); return; }

    if (eX) axis = (axis == Axis::X) ? Axis::None : Axis::X;
    if (eY) axis = (axis == Axis::Y) ? Axis::None : Axis::Y;
    if (eZ) axis = (axis == Axis::Z) ? Axis::None : Axis::Z;

    ImVec2 m = ImGui::GetIO().MousePos;
    glm::vec2 dm(m.x - startMouse.x, m.y - startMouse.y);
    float mul = shift ? 0.25f : 1.0f;
    applyDelta(app, dm, mul, ctrl);
}

/* =====================================================================
   GAT Paint Tool
   ===================================================================== */

class GATPaintTool : public ITool {
public:
    int radius = 0;                                    // default: 1x1 cell
    GatCell value = GatCell::NotWalkable;
    bool painting = false;
    std::vector<GatStrokeCommand::CellEdit> stroke;

    Scene* app_scene = nullptr;
    CommandStack* app_commands = nullptr;

    const char* name() const override { return "GAT Paint"; }
    const char* description() const override { return "Paint walkability cells."; }
    const char* statusHint() const override {
        return "LMB paint   |   Wheel radius   |   Right-click for quick settings";
    }

    bool hasQuickMenu() const override { return true; }
    int  brushCellRadius() const override { return radius; }
    // No heightmapBrushRadius() override: this tool paints the GAT grid,
    // not the heightmap, so it inherits the -1.0f default.

    void drawQuickMenu(Application&) override {
        ImGui::TextDisabled("GAT Brush");
        ImGui::SetNextItemWidth(180);
        ImGui::SliderInt("Radius", &radius, 0, 12);
        ImGui::TextDisabled("0 = 1x1 cell, 1 = 3x3 minus corners");
        int v = (int)value;
        const char* items[] = { "Walkable", "Not Walkable", "Event Walkable" };
        if (ImGui::Combo("Value", &v, items, 3)) value = (GatCell)v;
    }

    bool onMouseWheel(Application&, float delta) override {
        radius += (delta > 0 ? 1 : -1);
        radius = std::clamp(radius, 0, 12);
        return true;
    }

    void onMouseDown(Application& app, int button, float, float) override {
        if (button != 0) return;
        if (!app.hoveredGatCell.valid) return;
        painting = true;
        stroke.clear();
        paintAt(app.hoveredGatCell.x, app.hoveredGatCell.y);
    }
    void onMouseMove(Application& app, float, float) override {
        if (!painting || !app.hoveredGatCell.valid) return;
        paintAt(app.hoveredGatCell.x, app.hoveredGatCell.y);
    }
    void onMouseUp(Application&, int, float, float) override {
        if (!painting) return;
        painting = false;
        if (!stroke.empty() && app_scene && app_commands) {
            auto cmd = std::make_unique<GatStrokeCommand>(app_scene, std::move(stroke));
            app_commands->push(std::move(cmd));
        }
        stroke.clear();
    }
    void onDeactivate(Application& app) override {
        if (painting) onMouseUp(app, 0, 0.0f, 0.0f);
    }

    void onImGui(Application&) override {
        ImGui::SliderInt("Radius", &radius, 0, 12);
        ImGui::TextDisabled("0 = 1x1, 1 = plus shape, 2 = 13-cell circle");
        int v = (int)value;
        const char* items[] = { "Walkable", "Not Walkable", "Event Walkable" };
        if (ImGui::Combo("Value", &v, items, 3)) value = (GatCell)v;
    }

private:
    void paintAt(int cx, int cy) {
        if (!app_scene) return;
        for (int yy = cy - radius; yy <= cy + radius; ++yy) {
            for (int xx = cx - radius; xx <= cx + radius; ++xx) {
                if (!app_scene->inBounds(xx, yy)) continue;
                int dx = xx - cx, dy = yy - cy;
                if (dx * dx + dy * dy > radius * radius) continue;
                GatCell before = app_scene->at(xx, yy);
                if (before == value) continue;
                app_scene->at(xx, yy) = value;
                stroke.push_back({ xx, yy, before, value });
            }
        }
    }
};

/* =====================================================================
   Landscape Tool
   ===================================================================== */

class LandscapeTool : public ITool {
public:
    enum class Mode { Raise, Lower, Smooth, Flatten };
    Mode  mode = Mode::Raise;
    float radius = 3.0f;
    float strength = 0.35f;

    const char* name() const override { return "Landscape"; }
    const char* description() const override { return "Sculpt the heightmap."; }
    const char* statusHint() const override {
        return "LMB apply   |   Wheel radius   |   Right-click tool entry for mode/strength";
    }

    bool hasQuickMenu() const override { return true; }
    int  brushCellRadius() const override { return -1; }
    float heightmapBrushRadius() const override { return radius; }

    void drawQuickMenu(Application&) override {
        ImGui::TextDisabled("Landscape Brush");
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Radius", &radius, 0.5f, 16.0f, "%.1f");
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Strength", &strength, 0.02f, 1.0f, "%.2f");
        int m = (int)mode;
        const char* items[] = { "Raise", "Lower", "Smooth", "Flatten" };
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Mode", &m, items, 4)) mode = (Mode)m;
    }

    bool onMouseWheel(Application&, float delta) override {
        radius += (delta > 0 ? 0.5f : -0.5f);
        radius = std::clamp(radius, 0.5f, 16.0f);
        return true;
    }

    void onMouseDown(Application&, int button, float, float) override {
        if (button != 0) return;
        sculpting = true;
        edits.clear();
        editedVertices.clear();
        flattenValid = false;
    }
    void onMouseUp(Application& app, int, float, float) override {
        finishStroke(app);
    }
    void onDeactivate(Application& app) override { finishStroke(app); }

    void onUpdate(Application& app, float) override {
        if (sculpting) applyBrush(app, app.hoveredHmVertex);
    }

    void onImGui(Application&) override {
        int m = (int)mode;
        const char* items[] = { "Raise", "Lower", "Smooth", "Flatten" };
        if (ImGui::Combo("Mode", &m, items, 4)) mode = (Mode)m;
        ImGui::SliderFloat("Radius", &radius, 0.5f, 16.0f, "%.1f cells");
        ImGui::SliderFloat("Strength", &strength, 0.02f, 1.0f, "%.2f");
    }

private:
    bool  sculpting = false;
    bool  flattenValid = false;
    float flattenTarget = 0.0f;
    std::vector<HeightmapStrokeCommand::VertexEdit> edits;
    std::unordered_map<size_t, size_t> editedVertices;

    void finishStroke(Application& app) {
        if (!sculpting) return;
        sculpting = false;
        edits.erase(std::remove_if(edits.begin(), edits.end(),
                                   [](const auto& edit) {
                                       return edit.before == edit.after;
                                   }),
                    edits.end());
        if (!edits.empty()) {
            app.commands.push(std::make_unique<HeightmapStrokeCommand>(
                &app.scene, std::move(edits)));
            edits.clear();
            editedVertices.clear();
        }
        flattenValid = false;
    }

    void applyBrush(Application& app, const Application::HmHover& hov) {
        if (!hov.valid) return;
        Scene& s = app.scene;

        if (mode == Mode::Flatten && !flattenValid) {
            flattenTarget = s.hmAt(hov.x, hov.y);
            flattenValid = true;
        }

        auto stamp = [&](int x, int y) {
            float dx = (float)(x - hov.x);
            float dy = (float)(y - hov.y);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d > radius) return;
            float t = 1.0f - (d / radius);
            t = t * t * (3.0f - 2.0f * t);
            float& h = s.hmAt(x, y);
            const size_t index = (size_t)y * s.hmW + x;
            auto [editIt, inserted] = editedVertices.emplace(index, edits.size());
            if (inserted)
                edits.push_back({ x, y, h, h });

            switch (mode) {
                case Mode::Raise:  h += t * strength; break;
                case Mode::Lower:  h -= t * strength; break;
                case Mode::Smooth: {
                    float avg = 0.0f; int n = 0;
                    for (int oy = -1; oy <= 1; ++oy)
                        for (int ox = -1; ox <= 1; ++ox) {
                            int nx = x + ox, ny = y + oy;
                            if (s.hmInBounds(nx, ny)) { avg += s.hmAt(nx, ny); n++; }
                        }
                    if (n > 0) { avg /= n; h += (avg - h) * t * strength; }
                } break;
                case Mode::Flatten:
                    h += (flattenTarget - h) * t * strength;
                    break;
            }
            edits[editIt->second].after = h;
        };

        int r = (int)std::ceil(radius);
        for (int y = hov.y - r; y <= hov.y + r; ++y)
            for (int x = hov.x - r; x <= hov.x + r; ++x)
                if (s.hmInBounds(x, y)) stamp(x, y);

        s.heightmapVersion++;
    }
};

/* =====================================================================
   Texture Paint Tool
   ===================================================================== */

class TexturePaintTool : public ITool {
public:
    int radius = 0;                                    // default: 1x1 cell
    int layer = 1;
    bool painting = false;

    const char* name() const override { return "Texture Paint"; }
    const char* description() const override { return "Assign a texture layer index per cell."; }
    const char* statusHint() const override {
        return "LMB paint   |   Wheel radius   |   Right-click quick settings   |   L toggle overlay";
    }

    bool hasQuickMenu() const override { return true; }
    int  brushCellRadius() const override { return radius; }

    void drawQuickMenu(Application&) override {
        ImGui::TextDisabled("Texture Brush");
        ImGui::SetNextItemWidth(180);
        ImGui::SliderInt("Radius", &radius, 0, 8);
        ImGui::SetNextItemWidth(180);
        ImGui::SliderInt("Layer", &layer, 0, 7);
    }

    bool onMouseWheel(Application&, float delta) override {
        radius += (delta > 0 ? 1 : -1);
        radius = std::clamp(radius, 0, 8);
        return true;
    }

    void onMouseDown(Application& app, int button, float, float) override {
        if (button != 0) return;
        if (!app.hoveredGatCell.valid) return;
        painting = true;
        edits.clear();
        editedCells.clear();
        paintAt(app, app.hoveredGatCell.x, app.hoveredGatCell.y);
    }
    void onMouseMove(Application& app, float, float) override {
        if (!painting || !app.hoveredGatCell.valid) return;
        paintAt(app, app.hoveredGatCell.x, app.hoveredGatCell.y);
    }
    void onMouseUp(Application& app, int, float, float) override {
        finishStroke(app);
    }
    void onDeactivate(Application& app) override { finishStroke(app); }

    void onImGui(Application&) override {
        ImGui::SliderInt("Radius", &radius, 0, 8);
        ImGui::SliderInt("Layer", &layer, 0, 7);
    }

private:
    std::vector<TextureStrokeCommand::CellEdit> edits;
    std::unordered_set<size_t> editedCells;

    void finishStroke(Application& app) {
        if (!painting) return;
        painting = false;
        edits.erase(std::remove_if(edits.begin(), edits.end(),
                                   [](const auto& edit) {
                                       return edit.before == edit.after;
                                   }),
                    edits.end());
        if (!edits.empty()) {
            app.commands.push(std::make_unique<TextureStrokeCommand>(
                &app.scene, std::move(edits)));
            edits.clear();
            editedCells.clear();
        }
    }

    void paintAt(Application& app, int cx, int cy) {
        Scene& s = app.scene;
        for (int y = cy - radius; y <= cy + radius; ++y)
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (!s.inBounds(x, y)) continue;
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > radius * radius) continue;
                const size_t index = (size_t)y * s.gatW + x;
                uint8_t& cell = s.textureLayers[index];
                if (cell == (uint8_t)layer) continue;
                if (editedCells.insert(index).second)
                    edits.push_back({ x, y, cell, (uint8_t)layer });
                else {
                    for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
                        if (it->x == x && it->y == y) {
                            it->after = (uint8_t)layer;
                            break;
                        }
                    }
                }
                cell = (uint8_t)layer;
            }
    }
};

/* =====================================================================
   ToolManager
   ===================================================================== */

void ToolManager::init(Application& app) {
    this->app = &app;
    addTool(std::make_unique<SelectTool>());
    addTool(std::make_unique<TransformTool>());

    auto gat = std::make_unique<GATPaintTool>();
    gat->app_scene = &app.scene;
    gat->app_commands = &app.commands;
    addTool(std::move(gat));

    addTool(std::make_unique<LandscapeTool>());
    addTool(std::make_unique<TexturePaintTool>());

    activeIdx = 0;
    if (auto* t = active()) t->onActivate(app);
}

void ToolManager::shutdown() {
    tools.clear();
    app = nullptr;
}

ITool* ToolManager::active() const {
    if (activeIdx < 0 || activeIdx >= (int)tools.size()) return nullptr;
    return tools[activeIdx].get();
}

void ToolManager::setActive(int index) {
    if (index < 0 || index >= (int)tools.size()) return;
    if (index == activeIdx) return;
    if (app) {
        if (auto* t = active()) t->onDeactivate(*app);
        if (index >= GatPaintToolIndex && index <= TexturePaintToolIndex) {
            tools[TransformToolIndex]->onDeactivate(*app);
            app->selectedInstance = -1;
            app->gizmoDragAxis = 0;
            app->gizmoDragTargetId = -1;
            app->transformActive = false;
        }
    }
    activeIdx = index;
    if (app) {
        if (auto* t = active()) t->onActivate(*app);
    }
}

void ToolManager::update(Application& app, float dt) {
    for (size_t i = 0; i < tools.size(); ++i) {
        if ((int)i == activeIdx ||
            (activeIdx == SelectToolIndex && (int)i == TransformToolIndex))
            tools[i]->onUpdate(app, dt);
    }
}

void ToolManager::onImGui(Application& app, bool* open) {
    if (ImGui::Begin("Tools", open)) {
        for (int i = 0; i < (int)tools.size(); ++i) {
            bool sel = (i == activeIdx);
            ImGui::PushID(i);
            if (ImGui::Selectable(tools[i]->name(), sel)) setActive(i);

            if (tools[i]->hasQuickMenu()) {
                if (ImGui::BeginPopupContextItem("quick")) {
                    tools[i]->drawQuickMenu(app);
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        if (auto* t = active()) {
            ImGui::TextDisabled("%s", t->description());
            ImGui::Spacing();
            t->onImGui(app);
        }
    }
    ImGui::End();
}

void ToolManager::drawRadialMenu(Application& app) {
    (void)app;
    if (!radialOpen || tools.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    float inner = 70.0f, outer = 190.0f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddCircleFilled(center, outer, IM_COL32(15, 18, 24, 220), 64);
    dl->AddCircle(center, outer, IM_COL32(90, 120, 180, 200), 64, 2.0f);
    dl->AddCircle(center, inner, IM_COL32(90, 120, 180, 120), 64, 2.0f);

    ImVec2 m = io.MousePos;
    float dx = m.x - center.x, dy = m.y - center.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    int n = (int)tools.size();
    int hover = -1;
    if (dist >= inner && n > 0) {
        float ang = std::atan2(dy, dx) + 1.5707963f;
        if (ang < 0) ang += 6.2831853f;
        hover = (int)((ang / 6.2831853f) * n) % n;
    }
    radialHover = hover;

    for (int i = 0; i < n; ++i) {
        float a0 = (i / (float)n) * 6.2831853f - 1.5707963f;
        float a1 = ((i + 1) / (float)n) * 6.2831853f - 1.5707963f;
        float am = (a0 + a1) * 0.5f;

        ImU32 col = (i == hover) ? IM_COL32(80, 130, 210, 255)
                                 : IM_COL32(40, 48, 62, 200);
        dl->PathClear();
        int segs = 16;
        for (int s = 0; s <= segs; ++s) {
            float a = a0 + (a1 - a0) * (s / (float)segs);
            dl->PathLineTo(ImVec2(center.x + std::cos(a) * inner,
                                  center.y + std::sin(a) * inner));
        }
        for (int s = segs; s >= 0; --s) {
            float a = a0 + (a1 - a0) * (s / (float)segs);
            dl->PathLineTo(ImVec2(center.x + std::cos(a) * outer,
                                  center.y + std::sin(a) * outer));
        }
        dl->PathFillConvex(col);

        float lr = (inner + outer) * 0.5f;
        ImVec2 lp(center.x + std::cos(am) * lr - 30,
                  center.y + std::sin(am) * lr - 8);
        dl->AddText(lp, IM_COL32(240, 240, 240, 255), tools[i]->name());
    }

    dl->AddText(ImVec2(center.x - 20, center.y - 8),
                IM_COL32(200, 200, 200, 220), "Tools");
}

void ToolManager::drawStatusBar(Application& app) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float h = ImGui::GetFrameHeight() + 8.0f;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, h));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 1.0f));

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        if (app.transformActive) {
            const char* opName = (app.transformOp == 1) ? "GRAB"
                              : (app.transformOp == 2) ? "ROTATE"
                              : (app.transformOp == 3) ? "SCALE"
                                                       : "TRANSFORM";
            const char* axName = (app.transformAxis == 1) ? "X"
                              : (app.transformAxis == 2) ? "Y"
                              : (app.transformAxis == 3) ? "Z"
                                                         : "-";
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                               "  %s [%s]", opName, axName);
            ImGui::SameLine();
            ImGui::TextDisabled("| Ctrl snap   Shift precise   LMB/Enter confirm   Esc cancel");
        } else if (auto* t = active()) {
            ImGui::TextColored(ImVec4(0.62f, 0.85f, 1.0f, 1.0f), "  %s", t->name());
            ImGui::SameLine();
            ImGui::TextDisabled("| %s", t->statusHint());
        }

        char right[256];
        std::snprintf(right, sizeof(right),
                      "  Inst %d  |  Draws %d  |  Insts %d  |  Sel %d  |  Undo %d  Redo %d  ",
                      (int)app.scene.instances.size(),
                      app.renderer.lastDrawCalls,
                      app.renderer.lastInstanceCount,
                      app.selectedInstance,
                      (int)app.commands.undoSize(),
                      (int)app.commands.redoSize());

        float tw = ImGui::CalcTextSize(right).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - tw - 8.0f);
        ImGui::TextDisabled("%s", right);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}