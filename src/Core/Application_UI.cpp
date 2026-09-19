#include "Application.h"
#include "Save/SaveIO.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <filesystem>
#include <string>

void Application::drawUi() {
    drawDockHost();

    drawViewportWindow();
    tools.onImGui(*this);

    drawContentPanel();
    drawSceneListPanel();

    if (ImGui::Begin("Scene")) {
        ImGui::Text("Instances: %d", (int)scene.instances.size());
        ImGui::Text("Draw calls: %d", renderer.lastDrawCalls);
        ImGui::Text("Instances drawn: %d", renderer.lastInstanceCount);
        ImGui::Separator();
        ImGui::Checkbox("Show GAT overlay", &showGatOverlay);
        ImGui::Checkbox("Show texture overlay", &showTextureOverlay);

        ImGui::Separator();
        ImGui::TextDisabled("Selection");
        ImGui::Checkbox("Outline selected", &outlineEnabled);
        ImGui::SliderFloat("Outline thickness", &outlineThickness, 0.01f, 0.25f, "%.3f");
        if (selectedInstance > 0) {
            if (ImGui::Button("Focus (F)")) focusSelected();
            ImGui::SameLine();
            if (ImGui::Button("Duplicate (Ctrl+D)")) duplicateSelected();
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                scene.removeInstance(selectedInstance);
                selectedInstance = -1;
            }
        } else {
            ImGui::TextDisabled("Nothing selected.");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Snapping (Ctrl while transforming)");
        ImGui::SliderFloat("Translate", &gridSnapTranslate, 0.05f, 4.0f, "%.2f u");
        ImGui::SliderFloat("Rotate",    &gridSnapRotate,    1.0f, 90.0f, "%.0f deg");
        ImGui::SliderFloat("Scale",     &gridSnapScale,     0.01f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::TextDisabled("Camera wheel step");
        ImGui::SliderFloat("Wheel step", &wheelCamStep, 0.25f, 8.0f, "%.2f u");

        ImGui::Separator();
        if (ImGui::Button("Reset view (Num2)")) {
            camera.reset();
            pushToast("View reset");
        }
        ImGui::SameLine();
        if (ImGui::Button("Reseed cubes")) { seedScene(); pushToast("Reseeded"); }
        ImGui::SameLine();
        if (ImGui::Button("Clear Undo")) commands.clear();
    }
    ImGui::End();

    if (ImGui::Begin("Config")) {
        ImGui::TextDisabled("Hot-reload: edit any config/*.json");
        if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& r = config.get("render");
            if (r.contains("clearColor")) {
                auto& c = r["clearColor"];
                ImGui::Text("clearColor: %.2f %.2f %.2f",
                            c[0].get<float>(), c[1].get<float>(), c[2].get<float>());
            }
        }
        if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& t = config.get("tools");
            if (t.contains("gat"))
                ImGui::Text("gat.radius = %d", t["gat"].value("radius", 2));
        }
        if (ImGui::CollapsingHeader("Shortcuts", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& [name, code] : shortcuts.map()) {
                ImGui::Text("%-22s  %s", name.c_str(),
                            Shortcuts::keyName(code).c_str());
            }
        }
    }
    ImGui::End();

    if (ImGui::Begin("Materials")) {
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

    if (ImGui::Begin("Console")) {
        for (auto& t : toasts.items())
            ImGui::TextWrapped("[%d] %s", (int)t.level, t.message.c_str());
    }
    ImGui::End();

    float y = 40.0f;
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

    tools.drawRadialMenu(*this);
    tools.drawStatusBar(*this);

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
        ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down,  0.28f, nullptr, &main);

        ImGuiID leftTop    = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.55f, nullptr, &left);
        ImGuiID leftBottom = left;

        ImGui::DockBuilderDockWindow("Tools", leftTop);
        ImGui::DockBuilderDockWindow("Config", leftTop);
        ImGui::DockBuilderDockWindow("Scene List", leftBottom);
        ImGui::DockBuilderDockWindow("Scene", leftBottom);

        ImGui::DockBuilderDockWindow("Content", right);
        ImGui::DockBuilderDockWindow("Materials", right);
        ImGui::DockBuilderDockWindow("Console", bottom);
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
        if (ImGui::MenuItem("Save (F5)")) trySave();
        if (ImGui::MenuItem("Load (F9)")) tryLoad();
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
        if (ImGui::MenuItem("Undo", "Z", false, commands.canUndo())) commands.undo();
        if (ImGui::MenuItem("Redo", "Y", false, commands.canRedo())) commands.redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, selectedInstance > 0))
            duplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, selectedInstance > 0)) {
            scene.removeInstance(selectedInstance);
            selectedInstance = -1;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("GAT overlay", "F4", &showGatOverlay);
        ImGui::MenuItem("Texture overlay", "L", &showTextureOverlay);
        ImGui::MenuItem("Selection outline", nullptr, &outlineEnabled);
        ImGui::Separator();
        if (ImGui::MenuItem("Focus selection", "F", false, selectedInstance > 0))
            focusSelected();
        if (ImGui::MenuItem("Reset view", "Num2")) {
            camera.reset();
            pushToast("View reset");
        }
        if (ImGui::MenuItem("Reset layout")) {
            std::filesystem::remove("imgui.ini");
            dockBuilt = false;
        }
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("| Map-Maker");
    ImGui::EndMenuBar();
}