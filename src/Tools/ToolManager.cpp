#include "ToolManager.h"
#include "Core/Application.h"
#include "Core/Shortcuts.h"
#include "Core/Commands.h"
#include "Scene/Scene.h"
#include "Scene/Camera.h"
#include "Render/AssetImporter.h"
#include "Render/InstanceRenderer.h"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
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
                ImGui::Text("ID %d  (%s)", i->id, i->meshName.c_str());
                ImGui::DragFloat3("Position", &i->position.x, 0.05f);
                ImGui::DragFloat3("Rotation", &i->rotation.x, 0.5f);
                ImGui::DragFloat3("Scale",    &i->scale.x, 0.01f, 0.001f, 100.0f);
                ImGui::ColorEdit4("Tint", &i->tint.x);
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
    static int pick(Application& app, float mx, float my) {
        float vpW = app.viewportImageSize.x;
        float vpH = app.viewportImageSize.y;
        if (vpW <= 0.0f || vpH <= 0.0f) return -1;

        float originX = app.viewportImageMin.x;
        float originY = app.viewportImageMin.y;

        float aspect = vpW / vpH;
        glm::mat4 vp = app.camera.projection(aspect) * app.camera.view();

        int best = -1;
        float bestDist = 24.0f;

        for (auto& i : app.scene.instances) {
            glm::vec4 clip = vp * glm::vec4(i.position, 1.0f);
            if (clip.w <= 0.0001f) continue;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            float sx = originX + (ndc.x * 0.5f + 0.5f) * vpW;
            float sy = originY + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpH;
            float d = std::sqrt((sx - mx) * (sx - mx) + (sy - my) * (sy - my));
            if (d < bestDist) { bestDist = d; best = i.id; }
        }
        return best;
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
        Instance before;
        before.id = targetId;
        before.position = snapshotPos;
        before.rotation = snapshotRot;
        before.scale    = snapshotScale;

        Instance after = *inst;

        struct Cmd : Command {
            Scene* s; int id; Instance b, a;
            Cmd(Scene* sc, int id, Instance b, Instance a)
                : s(sc), id(id), b(b), a(a) {}
            void apply() override { if (auto* i = s->find(id)) *i = a; }
            void revert() override { if (auto* i = s->find(id)) *i = b; }
            const char* name() const override { return "Transform"; }
        };
        app.commands.push(std::make_unique<Cmd>(&app.scene, targetId, before, after));
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
        if (ImGui::GetIO().WantTextInput) return;
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
        if (button == 0) sculpting = true;
    }
    void onMouseUp(Application&, int, float, float) override { sculpting = false; }

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
        paintAt(app, app.hoveredGatCell.x, app.hoveredGatCell.y);
    }
    void onMouseMove(Application& app, float, float) override {
        if (!painting || !app.hoveredGatCell.valid) return;
        paintAt(app, app.hoveredGatCell.x, app.hoveredGatCell.y);
    }
    void onMouseUp(Application&, int, float, float) override { painting = false; }

    void onImGui(Application&) override {
        ImGui::SliderInt("Radius", &radius, 0, 8);
        ImGui::SliderInt("Layer", &layer, 0, 7);
    }

private:
    void paintAt(Application& app, int cx, int cy) {
        Scene& s = app.scene;
        for (int y = cy - radius; y <= cy + radius; ++y)
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (!s.inBounds(x, y)) continue;
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > radius * radius) continue;
                s.textureLayers[(size_t)y * s.gatW + x] = (uint8_t)layer;
            }
    }
};

/* =====================================================================
   Import Tool
   ===================================================================== */

class ImportTool : public ITool {
public:
    char path[512] = "Assets/model.obj";

    const char* name() const override { return "Import Asset"; }
    const char* description() const override { return "Load an .obj/.fbx/.gltf and place it."; }
    const char* statusHint() const override {
        return "Edit path, press Load   |   Or drag from the Content panel";
    }

    void onImGui(Application& app) override {
        ImGui::InputText("Path", path, sizeof(path));
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            doImport(app, path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...", ImVec2(140, 0))) {
            app.importModalOpen = true;
        }
        ImGui::TextDisabled("Uses assimp (obj/fbx/gltf/glb/dae/ply/stl).");
    }

    static bool doImport(Application& app, const std::string& p) {
        Mesh m;
        std::string err;
        if (!AssetImporter::loadMeshInto(p, m, err)) {
            app.pushToast("Import failed: " + err, ToastLevel::Error);
            return false;
        }
        size_t hash = AssetRegistry::hashPath(p);
        app.renderer.setMesh(hash, std::move(m));

        Instance inst;
        inst.meshHash = hash;
        inst.meshName = p;
        inst.meshPath = p;
        inst.position = { 0, 0, 0 };
        app.scene.addInstance(inst);
        app.pushToast("Imported " + p);
        return true;
    }
};

/* =====================================================================
   ToolManager
   ===================================================================== */

void ToolManager::init(Application& app) {
    addTool(std::make_unique<SelectTool>());
    addTool(std::make_unique<TransformTool>());

    auto gat = std::make_unique<GATPaintTool>();
    gat->app_scene = &app.scene;
    gat->app_commands = &app.commands;
    addTool(std::move(gat));

    addTool(std::make_unique<LandscapeTool>());
    addTool(std::make_unique<TexturePaintTool>());
    addTool(std::make_unique<ImportTool>());

    activeIdx = 0;
}

void ToolManager::shutdown() { tools.clear(); }

ITool* ToolManager::active() const {
    if (activeIdx < 0 || activeIdx >= (int)tools.size()) return nullptr;
    return tools[activeIdx].get();
}

void ToolManager::setActive(int index) {
    if (index < 0 || index >= (int)tools.size()) return;
    activeIdx = index;
}

void ToolManager::update(Application& app, float dt) {
    // Update every tool - the Transform tool must respond to G/R/S
    // regardless of which tool is currently selected.
    for (auto& t : tools) t->onUpdate(app, dt);
}

void ToolManager::onImGui(Application& app) {
    if (ImGui::Begin("Tools")) {
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