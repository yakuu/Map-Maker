#include "Application.h"
#include "Save/SaveIO.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

void Application::drawUi() {
    drawDockHost();

    if (showViewport) drawViewportWindow();
    if (showTools) tools.onImGui(*this, &showTools);

    if (showContent) drawContentPanel();
    if (showSceneList) drawSceneListPanel();

    if (showConfig) {
    if (ImGui::Begin("Config", &showConfig)) {
        ImGui::TextDisabled("Keybindings");
        std::vector<std::pair<std::string, int>> bindings(shortcuts.map().begin(),
                                                           shortcuts.map().end());
        std::sort(bindings.begin(), bindings.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        ImGui::TextDisabled("GLFW key names identify key positions; the printed letter varies by layout.");
        for (const auto& [name, code] : bindings) {
            const char* label = name.c_str();
            if (name == "edit.undo") label = "Undo (GLFW key)";
            if (name == "edit.redo") label = "Redo (GLFW key)";
            ImGui::Text("%-24s %s", label, Shortcuts::keyName(code).c_str());
        }
        ImGui::Text("%-24s Alt", "camera.lookAlt");
        ImGui::Text("%-24s F", "selection.focus");
        ImGui::Text("%-24s Ctrl+D", "edit.duplicate");
        ImGui::Text("%-24s G / R / S", "transform.moveRotateScale");
        ImGui::Text("%-24s X / Y / Z", "transform.axis");
        ImGui::Text("%-24s Enter / Esc / RMB", "transform.confirmCancel");

        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Instances: %d", (int)scene.instances.size());
            ImGui::Text("Draw calls: %d", renderer.lastDrawCalls);
            ImGui::Text("Instances drawn: %d", renderer.lastInstanceCount);
            ImGui::Separator();
            ImGui::Checkbox("Show GAT overlay", &showGatOverlay);
            ImGui::Checkbox("Show texture overlay", &showTextureOverlay);
            ImGui::Checkbox("Outline selected", &outlineEnabled);
            ImGui::SliderFloat("Outline thickness", &outlineThickness, 0.01f, 0.25f, "%.3f");
            if (selectedInstance > 0) {
                if (ImGui::Button("Focus (F)")) focusSelected();
                ImGui::SameLine();
                if (ImGui::Button("Duplicate (Ctrl+D)")) duplicateSelected();
                ImGui::SameLine();
                if (ImGui::Button("Delete")) {
                    if (auto* instance = scene.find(selectedInstance)) {
                        const Instance deleted = *instance;
                        const size_t index = scene.indexOf(selectedInstance);
                        scene.removeInstance(selectedInstance);
                        commands.push(std::make_unique<InstancePresenceCommand>(
                            &scene, deleted, index, false, "Delete Instance"));
                    }
                    selectedInstance = -1;
                }
            } else {
                ImGui::TextDisabled("Nothing selected.");
            }

            ImGui::Separator();
            ImGui::TextDisabled("Snapping (Ctrl while transforming)");
            ImGui::SliderFloat("Translate", &gridSnapTranslate, 0.05f, 4.0f, "%.2f u");
            ImGui::SliderFloat("Rotate", &gridSnapRotate, 1.0f, 90.0f, "%.0f deg");
            ImGui::SliderFloat("Scale", &gridSnapScale, 0.01f, 1.0f, "%.2f");
            ImGui::SliderFloat("Camera wheel step", &wheelCamStep, 0.25f, 8.0f, "%.2f u");
            if (ImGui::Button("Reset view (Num2)")) {
                camera.reset();
                pushToast("View reset");
            }
            ImGui::SameLine();
            if (ImGui::Button("Reseed cubes")) {
                const size_t firstNew = scene.instances.size();
                seedScene();
                std::vector<Instance> added(scene.instances.begin() + (std::ptrdiff_t)firstNew,
                                            scene.instances.end());
                commands.push(std::make_unique<InstanceBatchPresenceCommand>(
                    &scene, std::move(added), firstNew, true));
                pushToast("Reseeded");
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Undo")) commands.clear();
        }

        ImGui::TextDisabled("Hot-reload: edit any config/*.json");
        if (ImGui::CollapsingHeader("Render")) {
            auto& r = config.get("render");
            if (r.contains("clearColor")) {
                auto& c = r["clearColor"];
                ImGui::Text("clearColor: %.2f %.2f %.2f",
                            c[0].get<float>(), c[1].get<float>(), c[2].get<float>());
            }
        }
        if (ImGui::CollapsingHeader("Tools")) {
            auto& t = config.get("tools");
            if (t.contains("gat"))
                ImGui::Text("gat.radius = %d", t["gat"].value("radius", 2));
        }
    }
    ImGui::End();
    }

    if (showMaterials) {
    if (ImGui::Begin("Materials", &showMaterials)) {
        auto& m = config.get("materials");
        if (m.contains("materials")) {
            for (auto& [k, v] : m["materials"].items()) {
                ImGui::Text("%s", k.c_str());
                if (v.contains("albedo") && v["albedo"].is_array()) {
                    auto& a = v["albedo"];
                    ImGui::ColorButton(("##" + k).c_str(),
                                       ImVec4(a[0].get<float>(), a[1].get<float>(),
                                              a[2].get<float>(), 1.0f));
                    ImGui::SameLine();
                    ImGui::TextDisabled("rough=%.2f metal=%.2f",
                        v.value("roughness", 0.5f), v.value("metalness", 0.0f));
                }
            }
        }
    }
    ImGui::End();
    }

    if (showConsole) {
    if (ImGui::Begin("Console", &showConsole)) {
        if (ImGui::Button("Clear")) consoleLog.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("%d log entries", (int)consoleLog.items().size());
        ImGui::Separator();
        ImGui::BeginChild("##ConsoleLog", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        const bool wasAtBottom =
            ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        for (const auto& entry : consoleLog.items()) {
            ImVec4 color(0.82f, 0.84f, 0.88f, 1.0f);
            const char* level = "INFO";
            if (entry.level == ToastLevel::Warning) {
                color = ImVec4(1.0f, 0.78f, 0.24f, 1.0f);
                level = "WARN";
            }
            if (entry.level == ToastLevel::Error) {
                color = ImVec4(1.0f, 0.38f, 0.35f, 1.0f);
                level = "ERROR";
            }
            ImGui::TextColored(color, "[%s] %s", level, entry.message.c_str());
        }
        if (wasAtBottom) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();
    }

    float y = 40.0f;
    if (showNotifications) {
        for (auto& t : toasts.items()) {
            ImVec4 col(0.2f, 0.2f, 0.2f, 0.9f);
            if (t.level == ToastLevel::Warning) col = ImVec4(0.55f, 0.4f, 0.1f, 0.92f);
            if (t.level == ToastLevel::Error)   col = ImVec4(0.55f, 0.15f, 0.15f, 0.92f);
            ImGui::SetNextWindowBgAlpha(0.92f);
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 360, y));
            ImGui::SetNextWindowSize(ImVec2(340, 0));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
            ImGui::Begin(("##toast" + t.message).c_str(), nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextWrapped("%s", t.message.c_str());
            ImGui::End();
            ImGui::PopStyleColor();
            y += 48.0f;
        }
    }

    tools.drawRadialMenu(*this);
    if (showStatusBar) tools.drawStatusBar(*this);

    drawImportModal();
    drawContentFolderModal();
}

void Application::drawDockHost() {
    ImGuiViewport* vp = ImGui::GetMainViewport();

    const float statusH = ImGui::GetFrameHeight() + 8.0f;
    ImVec2 hostPos(vp->WorkPos.x, vp->WorkPos.y);
    ImVec2 hostSize(vp->WorkSize.x, vp->WorkSize.y - statusH);

    ImGui::SetNextWindowPos(hostPos);
    ImGui::SetNextWindowSize(hostSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    drawMenuBar();

    ImGuiID dockId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!dockBuilt) {
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, hostSize);

        ImGuiID main = dockId;
        ImGuiID left   = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left,  0.20f, nullptr, &main);
        ImGuiID right  = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.24f, nullptr, &main);
        ImGuiID leftTop    = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.55f, nullptr, &left);
        ImGuiID leftBottom = left;

        ImGui::DockBuilderDockWindow("Tools", leftTop);
        ImGui::DockBuilderDockWindow("Config", leftTop);
        ImGui::DockBuilderDockWindow("Scene List", leftBottom);

        ImGui::DockBuilderDockWindow("Content", right);
        ImGui::DockBuilderDockWindow("Materials", right);
        ImGui::DockBuilderDockWindow("Viewport", main);

        ImGui::DockBuilderFinish(dockId);
        dockBuilt = true;
    }

    ImGui::End();
}

void Application::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Import Asset...")) importModalOpen = true;
        if (ImGui::MenuItem("Load Folder...")) contentFolderModalOpen = true;
        ImGui::Separator();
        const std::string saveKey = Shortcuts::keyName(shortcuts.key("save.quick"));
        if (ImGui::MenuItem(("Save (" + saveKey + ")").c_str())) trySave();
        const std::string loadKey = Shortcuts::keyName(shortcuts.key("load.quick"));
        if (ImGui::MenuItem(("Load (" + loadKey + ")").c_str())) tryLoad();
        if (ImGui::MenuItem("Export Map...")) {
            std::filesystem::create_directories("Save");
            std::string p = "Save/export_" + std::to_string((int)glfwGetTime()) + ".json";
            if (SaveIO::save(p, scene)) pushToast("Exported " + p);
            else                        pushToast("Export failed", ToastLevel::Error);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) glfwSetWindowShouldClose(window, 1);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const std::string undoKey = Shortcuts::keyName(shortcuts.key("edit.undo"));
        const std::string redoKey = Shortcuts::keyName(shortcuts.key("edit.redo"));
        if (ImGui::MenuItem(("Undo (" + undoKey + " key)").c_str(), nullptr,
                            false, commands.canUndo())) commands.undo();
        if (ImGui::MenuItem(("Redo (" + redoKey + " key)").c_str(), nullptr,
                            false, commands.canRedo())) commands.redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, selectedInstance > 0))
            duplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, selectedInstance > 0)) {
            if (auto* instance = scene.find(selectedInstance)) {
                const Instance deleted = *instance;
                const size_t index = scene.indexOf(selectedInstance);
                scene.removeInstance(selectedInstance);
                commands.push(std::make_unique<InstancePresenceCommand>(
                    &scene, deleted, index, false, "Delete Instance"));
            }
            selectedInstance = -1;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        const std::string gatKey = Shortcuts::keyName(shortcuts.key("view.toggleGat"));
        const std::string textureKey =
            Shortcuts::keyName(shortcuts.key("view.toggleLandscape"));
        ImGui::MenuItem("GAT overlay", gatKey.c_str(), &showGatOverlay);
        ImGui::MenuItem("Texture overlay", textureKey.c_str(), &showTextureOverlay);
        ImGui::MenuItem("Selection outline", nullptr, &outlineEnabled);
        ImGui::Separator();
        if (ImGui::MenuItem("Focus selection", "F", false, selectedInstance > 0))
            focusSelected();
        if (ImGui::MenuItem("Reset view", "Num2")) {
            camera.reset();
            pushToast("View reset");
        }
        if (ImGui::MenuItem("Reset layout")) {
            std::filesystem::remove(kImGuiLayoutPath);
            dockBuilt = false;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Panels");
        ImGui::MenuItem("Viewport", nullptr, &showViewport);
        ImGui::MenuItem("Tools", nullptr, &showTools);
        ImGui::MenuItem("Content", nullptr, &showContent);
        ImGui::MenuItem("Scene List", nullptr, &showSceneList);
        ImGui::MenuItem("Config", nullptr, &showConfig);
        ImGui::MenuItem("Materials", nullptr, &showMaterials);
        ImGui::MenuItem("Console", nullptr, &showConsole);
        ImGui::MenuItem("Status Bar", nullptr, &showStatusBar);
        ImGui::MenuItem("Notifications", nullptr, &showNotifications);
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("| Map-Maker");
    if (ImGui::BeginMenu("Tools")) {
        for (int i = 0; i < (int)tools.all().size(); ++i) {
            const std::string action = "tool." +
                std::string(i == 0 ? "select" : i == 1 ? "transform" :
                            i == 2 ? "gatPaint" : i == 3 ? "landscape" :
                                     "texturePaint");
            const std::string keyName = Shortcuts::keyName(shortcuts.key(action));
            if (ImGui::MenuItem(tools.all()[i]->name(), keyName.c_str(),
                                tools.activeIndex() == i))
                tools.setActive(i);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Terrain")) {
    if (ImGui::MenuItem("Small  (33 x 33)"))  resizeHeightmap(33,  33);
    if (ImGui::MenuItem("Medium (65 x 65)"))  resizeHeightmap(65,  65);
    if (ImGui::MenuItem("Large  (129 x 129)")) resizeHeightmap(129, 129);
    if (ImGui::MenuItem("Huge   (257 x 257)")) resizeHeightmap(257, 257);
    ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

}