#include "Application.h"
#include "ApplicationInternal.h"
#include "Render/AssetImporter.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

/* -------------------------------------------------------------------------
   Content panel
   ------------------------------------------------------------------------- */

void Application::drawContentPanel() {
    if (!ImGui::Begin("Content")) { ImGui::End(); return; }

    if (ImGui::Button("Load Folder...")) contentFolderModalOpen = true;
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) {
        int n = assets.scan(assets.root());
        pushToast("Scanned " + std::to_string(n) + " asset(s)");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%d asset%s)",
                        (int)assets.entries().size(),
                        assets.entries().size() == 1 ? "" : "s");

    struct FilterDef { const char* label; const char* ext; };
    static const FilterDef kFilters[] = {
        { "All",   ""      },
        { ".obj",  ".obj"  },
        { ".fbx",  ".fbx"  },
        { ".gltf", ".gltf" },
        { ".glb",  ".glb"  },
        { ".dae",  ".dae"  },
        { ".ply",  ".ply"  },
        { ".stl",  ".stl"  },
        { ".3ds",  ".3ds"  },
    };

    ImGui::TextDisabled("Filter:");
    ImGui::SameLine();

    for (size_t i = 0; i < sizeof(kFilters)/sizeof(kFilters[0]); ++i) {
        const auto& f = kFilters[i];
        bool active = (contentFilterExt == f.ext);

        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.28f, 0.52f, 0.85f, 1.0f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.18f, 0.20f, 0.24f, 1.0f));

        if (ImGui::SmallButton(f.label))
            contentFilterExt = f.ext;

        ImGui::PopStyleColor();

        if (i + 1 < sizeof(kFilters)/sizeof(kFilters[0]))
            ImGui::SameLine();
    }

    ImGui::Separator();

    int visibleCount = 0;
    for (auto& a : assets.entries()) {
        if (!contentFilterExt.empty() &&
            AppInternal::lowerExt(a.fullPath) != contentFilterExt)
            continue;
        visibleCount++;
    }

    if (visibleCount == 0)
        ImGui::TextDisabled("No assets match the current filter.");

    ImGui::BeginChild("##contentList", ImVec2(0, 0), true);
    for (auto& a : assets.entries()) {
        if (!contentFilterExt.empty() &&
            AppInternal::lowerExt(a.fullPath) != contentFilterExt)
            continue;

        ImGui::PushID((int)a.hash);

        char label[512];
        std::snprintf(label, sizeof(label), "%s%s",
                      a.loaded ? "" : "[!] ", a.path.c_str());

        if (ImGui::Selectable(label)) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (!a.loaded) {
                    Mesh m; std::string err;
                    if (AssetImporter::loadMeshInto(a.fullPath, m, err)) {
                        renderer.setMesh(a.hash, std::move(m));
                        a.loaded = true;
                    }
                }
                if (renderer.getMesh(a.hash)) {
                    Instance inst;
                    inst.meshHash = a.hash;
                    inst.meshName = a.name;
                    inst.position = { 0, 0, 0 };
                    scene.addInstance(inst);
                    pushToast("Placed " + a.name);
                }
            }
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            size_t h = a.hash;
            ImGui::SetDragDropPayload("ASSET_HASH", &h, sizeof(h));
            ImGui::Text("Place %s", a.name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%d indices", a.fullPath.c_str(), a.indexCount);

        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::End();
}

/* -------------------------------------------------------------------------
   Scene List panel
   ------------------------------------------------------------------------- */

void Application::drawSceneListPanel() {
    if (!ImGui::Begin("Scene List")) { ImGui::End(); return; }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNodeEx("Obj", ImGuiTreeNodeFlags_DefaultOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth)) {
        std::map<std::string, std::vector<Instance*>> groups;
        for (auto& inst : scene.instances)
            groups[inst.meshName.empty() ? std::string("(unnamed)") : inst.meshName]
                .push_back(&inst);

        if (groups.empty()) ImGui::TextDisabled("  (no objects)");

        for (auto& [name, list] : groups) {
            std::sort(list.begin(), list.end(),
                      [](const Instance* a, const Instance* b) {
                          return a->id < b->id;
                      });

            std::string groupLabel = name + " (" + std::to_string(list.size()) + ")";
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::TreeNodeEx(groupLabel.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth)) {

                for (auto* inst : list) {
                    char item[256];
                    std::snprintf(item, sizeof(item), "%s%d",
                                  name.c_str(), inst->id);

                    bool sel = (selectedInstance == inst->id);
                    if (ImGui::Selectable(item, sel))
                        selectedInstance = inst->id;

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Focus")) {
                            selectedInstance = inst->id;
                            focusSelected();
                        }
                        if (ImGui::MenuItem("Duplicate")) {
                            selectedInstance = inst->id;
                            duplicateSelected();
                        }
                        if (ImGui::MenuItem("Delete")) {
                            int delId = inst->id;
                            scene.removeInstance(delId);
                            if (selectedInstance == delId)
                                selectedInstance = -1;
                            ImGui::EndPopup();
                            break;
                        }
                        ImGui::EndPopup();
                    }
                }

                ImGui::TreePop();
            }
        }

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNodeEx("Landscape", ImGuiTreeNodeFlags_DefaultOpen |
                                        ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::TextDisabled("  Grid:   %d x %d  (cell %.2f)",
                            scene.hmW, scene.hmH, scene.hmCell);
        ImGui::TextDisabled("  Origin: %.1f, %.1f, %.1f",
                            scene.hmOrigin.x, scene.hmOrigin.y, scene.hmOrigin.z);

        bool live = (renderedHeightmapVersion == scene.heightmapVersion);
        ImGui::TextDisabled("  Mesh:   %s", live ? "up to date" : "rebuilding");

        ImGui::Checkbox("Show texture overlay", &showTextureOverlay);
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset heightmap")) {
            std::fill(scene.heightmap.begin(), scene.heightmap.end(), 0.0f);
            scene.heightmapVersion++;
            pushToast("Heightmap reset");
        }

        ImGui::TreePop();
    }

    ImGui::End();
}

/* -------------------------------------------------------------------------
   Modals
   ------------------------------------------------------------------------- */

void Application::drawImportModal() {
    if (importModalOpen) {
        ImGui::OpenPopup("Import Asset");
        importModalOpen = false;
    }
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Import Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Path to .obj / .fbx / .gltf / .glb / .dae / .ply / .stl:");
        ImGui::SetNextItemWidth(520);
        ImGui::InputText("##path", importPath, sizeof(importPath));

        ImGui::Spacing();
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            Mesh m;
            std::string err;
            if (AssetImporter::loadMeshInto(importPath, m, err)) {
                size_t hash = AssetRegistry::hashPath(std::string(importPath));
                renderer.setMesh(hash, std::move(m));
                Instance inst;
                inst.meshHash = hash;
                inst.meshName = importPath;
                inst.position = { 0, 0, 0 };
                scene.addInstance(inst);
                pushToast(std::string("Imported ") + importPath);
            } else {
                pushToast("Import failed: " + err, ToastLevel::Error);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void Application::drawContentFolderModal() {
    if (contentFolderModalOpen) {
        ImGui::OpenPopup("Load Folder");
        contentFolderModalOpen = false;
    }
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Load Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Folder to scan recursively for meshes:");
        ImGui::SetNextItemWidth(520);
        ImGui::InputText("##folder", contentFolder, sizeof(contentFolder));

        ImGui::Spacing();
        if (ImGui::Button("Scan", ImVec2(120, 0))) {
            int n = assets.scan(contentFolder);
            pushToast("Found " + std::to_string(n) + " mesh file(s) in " + contentFolder);
            preloadAssets();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}