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
        return "LMB: pick   |   RMB on viewport: look   |   Z: undo";
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
            } else {
                app.selectedInstance = -1;
            }
        } else {
            ImGui::TextDisabled("Nothing selected.");
        }
    }

private:
    static int pick(Application& app, float mx, float my) {
        // rebuild viewport rect from window position
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 cur = ImGui::GetCursorScreenPos();

        float vpW = (float)app.viewportFbo.width();
        float vpH = (float)app.viewportFbo.height();
        if (vpW <= 0 || vpH <= 0) return -1;

        float originX = cur.x;
        float originY = cur.y;

        float aspect = vpW / vpH;
        glm::mat4 vp = app.camera.projection(aspect) * app.camera.view();

        int best = -1;
        float bestDist = 24.0f; // px

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
   GAT Paint Tool
   ===================================================================== */

class GATPaintTool : public ITool {
public:
    int radius = 2;
    GatCell value = GatCell::NotWalkable;
    bool painting = false;
    std::vector<GatStrokeCommand::CellEdit> stroke;

    Scene* app_scene = nullptr;
    CommandStack* app_commands = nullptr;

    const char* name() const override { return "GAT Paint"; }
    const char* description() const override { return "Paint walkability cells."; }
    const char* statusHint() const override {
        return "LMB: paint   |   Tool panel: radius & value   |   Z: undo";
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
        ImGui::SliderInt("Radius", &radius, 0, 8);
        int v = (int)value;
        const char* items[] = { "Walkable", "Not Walkable", "Event Walkable" };
        if (ImGui::Combo("Value", &v, items, 3)) value = (GatCell)v;
    }

private:
    void paintAt(int cx, int cy) {
        if (!app_scene) return;
        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if (!app_scene->inBounds(x, y)) continue;
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > radius * radius) continue;
                GatCell before = app_scene->at(x, y);
                if (before == value) continue;
                app_scene->at(x, y) = value;
                stroke.push_back({ x, y, before, value });
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
    Mode mode = Mode::Raise;
    float radius = 3.0f;   // in cells
    float strength = 0.35f;

    const char* name() const override { return "Landscape"; }
    const char* description() const override { return "Sculpt the heightmap."; }
    const char* statusHint() const override {
        return "LMB: apply   |   Shift: invert   |   1/2/3/4: mode   |   Tool panel: radius & strength";
    }

    void onMouseDown(Application&, int button, float, float) override {
        if (button == 0) sculpting = true;
    }
    void onMouseUp(Application&, int, float, float) override { sculpting = false; }

    void onUpdate(Application& app, float) override {
        if (sculpting) applyBrush(app, app.hoveredHmVertex, app.lastHeightmapEdit);
        else app.lastHeightmapEdit = false;
    }

    void onImGui(Application&) override {
        int m = (int)mode;
        const char* items[] = { "Raise", "Lower", "Smooth", "Flatten" };
        if (ImGui::Combo("Mode", &m, items, 4)) mode = (Mode)m;
        ImGui::SliderFloat("Radius", &radius, 0.5f, 12.0f, "%.1f cells");
        ImGui::SliderFloat("Strength", &strength, 0.02f, 1.0f, "%.2f");
    }

private:
    bool sculpting = false;

    void applyBrush(Application& app, const Application::HmHover& hov, bool& dirty) {
        dirty = false;
        if (!hov.valid) return;
        Scene& s = app.scene;

        auto stamp = [&](int x, int y, float amount) {
            float dx = (float)(x - hov.x);
            float dy = (float)(y - hov.y);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d > radius) return;
            float t = 1.0f - (d / radius);
            t = t * t * (3.0f - 2.0f * t); // smoothstep falloff
            float& h = s.hmAt(x, y);

            switch (mode) {
                case Mode::Raise:  h += amount * t * strength; break;
                case Mode::Lower:  h -= amount * t * strength; break;
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

        if (mode == Mode::Flatten && !flattenValid) {
            flattenTarget = s.hmAt(hov.x, hov.y);
            flattenValid = true;
        }

        int r = (int)std::ceil(radius);
        for (int y = hov.y - r; y <= hov.y + r; ++y)
            for (int x = hov.x - r; x <= hov.x + r; ++x)
                if (s.hmInBounds(x, y)) stamp(x, y, 1.0f);

        s.heightmapVersion++;
        dirty = true;
    }

    bool  flattenValid = false;
    float flattenTarget = 0.0f;
};

/* =====================================================================
   Texture Paint Tool
   ===================================================================== */

class TexturePaintTool : public ITool {
public:
    int radius = 2;
    int layer = 1;
    bool painting = false;

    const char* name() const override { return "Texture Paint"; }
    const char* description() const override { return "Assign a texture layer index per cell."; }
    const char* statusHint() const override {
        return "LMB: paint   |   Tool panel: layer index & radius";
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
    bool placeImported = true;

    const char* name() const override { return "Import Asset"; }
    const char* description() const override { return "Load an .obj/.fbx/.gltf and place it."; }
    const char* statusHint() const override {
        return "Edit path, press Load  |   File: Import Asset… (also in menu bar)";
    }

    void onImGui(Application& app) override {
        ImGui::InputText("Path", path, sizeof(path));
        ImGui::Checkbox("Place at origin after load", &placeImported);
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            doImport(app, path, placeImported);
        }
        ImGui::SameLine();
        if (ImGui::Button("File dialog…", ImVec2(140, 0))) {
            app.importModalOpen = true;
        }
        ImGui::TextDisabled("Uses assimp (obj/fbx/gltf/glb/dae/ply/stl).");
    }

    static bool doImport(Application& app, const std::string& p, bool place) {
        Mesh m;
        std::string err;
        if (!AssetImporter::loadMeshInto(p, m, err)) {
            app.pushToast("Import failed: " + err, ToastLevel::Error);
            return false;
        }
        size_t hash = std::hash<std::string>{}(p);
        app.renderer.setMesh(hash, std::move(m));

        if (place) {
            Instance inst;
            inst.meshHash = hash;
            inst.meshName = p;
            inst.position = { 0, 0, 0 };
            app.scene.addInstance(inst);
        }
        app.pushToast("Imported " + p);
        return true;
    }
};

/* =====================================================================
   ToolManager
   ===================================================================== */

void ToolManager::init(Application& app) {
    auto select = std::make_unique<SelectTool>();
    addTool(std::move(select));

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
    if (auto* t = active()) t->onUpdate(app, dt);
}

void ToolManager::onImGui(Application& app) {
    if (ImGui::Begin("Tools")) {
        for (int i = 0; i < (int)tools.size(); ++i) {
            bool sel = (i == activeIdx);
            if (ImGui::Selectable(tools[i]->name(), sel)) setActive(i);
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
        if (auto* t = active()) {
            ImGui::TextColored(ImVec4(0.62f, 0.85f, 1.0f, 1.0f), "  %s", t->name());
            ImGui::SameLine();
            ImGui::TextDisabled("| %s", t->statusHint());
        }

        // right-hand side info
        char right[256];
        std::snprintf(right, sizeof(right),
                      "  Inst %d  |  Draws %d  |  Insts %d  |  Undo %d  Redo %d  ",
                      (int)app.scene.instances.size(),
                      app.renderer.lastDrawCalls,
                      app.renderer.lastInstanceCount,
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