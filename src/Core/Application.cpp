#include "Application.h"
#include "Save/SaveIO.h"
#include "Render/AssetImporter.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace {
void glfwErrorCb(int code, const char* desc) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, desc);
}
}

bool Application::init() {
    glfwSetErrorCallback(glfwErrorCb);
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return false; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    window = glfwCreateWindow(1600, 900, "Map-Maker", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "window creation failed\n"); return false; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!LoadGLFunctions()) {
        std::fprintf(stderr, "GL load failed: %s\n", LastGLLoadError());
        return false;
    }

    std::printf("[gl] %s | %s\n",
                (const char*)glGetString(GL_VERSION),
                (const char*)glGetString(GL_RENDERER));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Only write imgui.ini if it doesn't exist yet (so we can lay out programmatically first time)
    bool hadIni = std::filesystem::exists("imgui.ini");
    io.IniFilename = "imgui.ini";
    dockBuilt = hadIni;   // if it exists, we trust user's saved layout

    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 3.0f;
    st.FrameRounding = 3.0f;
    st.GrabRounding = 3.0f;
    st.WindowBorderSize = 1.0f;
    st.FrameBorderSize = 0.0f;
    st.WindowPadding = ImVec2(10, 10);
    st.FramePadding = ImVec2(8, 5);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    config.loadDirectory("config");
    shortcuts.load("config/shortcuts.json");
    config.onReload = [this](const std::string& name) {
        pushToast("Reloaded config: " + name, ToastLevel::Info);
    };

    if (!renderer.init()) { pushToast("Renderer init failed", ToastLevel::Error); return false; }

    viewportFbo.resize(viewportW, viewportH);
    scene.initGat(64, 64);
    scene.initHeightmap(33, 33);
    seedScene();

    tools.init(*this);

    glfwShowWindow(window);
    lastTime = glfwGetTime();
    pushToast("Map-Maker ready", ToastLevel::Info);
    return true;
}

void Application::seedScene() {
    Mesh cube = MeshFactory::makeCube(1.0f);
    cubeHash = 0xC0FFEEull;
    renderer.ensureMesh(cubeHash, std::move(cube));

    for (int x = -3; x <= 3; ++x) {
        for (int z = -3; z <= 3; ++z) {
            Instance i;
            i.meshHash = cubeHash;
            i.meshName = "cube";
            i.position = { (float)x * 1.6f, 1.2f, (float)z * 1.6f };
            i.tint = glm::vec4(0.6f + 0.05f * x, 0.7f, 0.5f + 0.05f * z, 1.0f);
            scene.addInstance(i);
        }
    }
}

void Application::run() {
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        if (dt > 0.25f) dt = 0.25f;
        frame(dt);
    }
}

bool Application::viewportHoveredForCamera() const {
    return ImGui::GetIO().WantCaptureMouse == false;
}

void Application::frame(float dt) {
    glfwPollEvents();

    config.poll();
    shortcuts.update(window);
    toasts.update();
    handleGlobalKeys();

    bool captured = cursorCaptured;
    if (captured) {
        if (shortcuts.justReleased("camera.capture")) {
            cursorCaptured = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    } else {
        if (shortcuts.justPressed("camera.capture") && viewportHoveredForCamera()) {
            cursorCaptured = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    camera.update(window, dt, captured,
                  shortcuts.isDown("camera.forward"),
                  shortcuts.isDown("camera.back"),
                  shortcuts.isDown("camera.left"),
                  shortcuts.isDown("camera.right"),
                  shortcuts.isDown("camera.up"),
                  shortcuts.isDown("camera.down"),
                  shortcuts.isDown("camera.fast"));

    tools.update(*this, dt);

    // rebuild heightmap if dirty
    if (scene.heightmapVersion != renderedHeightmapVersion)
        rebuildHeightmapMesh();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawUi();

    bool tab = shortcuts.isDown("tool.radial");
    if (tab && !tools.radialOpen) tools.radialOpen = true;
    if (!tab && tools.radialOpen) {
        if (tools.radialHover >= 0) tools.setActive(tools.radialHover);
        tools.radialOpen = false;
    }

    ImGui::Render();

    drawScene();

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

void Application::rebuildHeightmapMesh() {
    Scene& s = scene;
    if (s.heightmap.empty()) return;

    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    verts.reserve((size_t)s.hmW * s.hmH);

    auto H = [&](int x, int y) {
        x = std::clamp(x, 0, s.hmW - 1);
        y = std::clamp(y, 0, s.hmH - 1);
        return s.hmAt(x, y);
    };

    for (int y = 0; y < s.hmH; ++y) {
        for (int x = 0; x < s.hmW; ++x) {
            float h = s.hmAt(x, y);
            glm::vec3 p = s.hmOrigin + glm::vec3(x * s.hmCell, h, y * s.hmCell);

            float hl = H(x - 1, y), hr = H(x + 1, y);
            float hd = H(x, y - 1), hu = H(x, y + 1);
            glm::vec3 n = glm::normalize(glm::vec3(
                -(hr - hl) / (2.0f * s.hmCell),
                 1.0f,
                -(hu - hd) / (2.0f * s.hmCell)));

            glm::vec2 uv((float)x / (float)(s.hmW - 1),
                         (float)y / (float)(s.hmH - 1));
            verts.push_back({ p, n, uv });
        }
    }

    for (int y = 0; y < s.hmH - 1; ++y) {
        for (int x = 0; x < s.hmW - 1; ++x) {
            uint32_t i0 = (uint32_t)(y * s.hmW + x);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + s.hmW;
            uint32_t i3 = i2 + 1;
            idx.insert(idx.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }

    Mesh m;
    m.upload(verts, idx);
    m.name = "heightmap";
    renderer.setMesh(heightmapHash, std::move(m));
    renderedHeightmapVersion = s.heightmapVersion;
}

void Application::drawScene() {
    viewportFbo.bind();
    glViewport(0, 0, viewportFbo.width(), viewportFbo.height());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    renderer.begin(vp);

    // heightmap first
    {
        glm::mat4 m(1.0f);
        renderer.submit(heightmapHash, m, glm::vec4(0.35f, 0.55f, 0.3f, 1.0f));
    }

    for (auto& i : scene.instances) {
        glm::mat4 m(1.0f);
        m = glm::translate(m, i.position);
        m = glm::rotate(m, glm::radians(i.rotation.x), { 1,0,0 });
        m = glm::rotate(m, glm::radians(i.rotation.y), { 0,1,0 });
        m = glm::rotate(m, glm::radians(i.rotation.z), { 0,0,1 });
        m = glm::scale(m, i.scale);
        renderer.submit(i.meshHash, m, i.tint);
    }
    renderer.end();

    viewportFbo.unbind();
}

void Application::drawUi() {
    // 1) main dock host (leaves room for status bar at bottom)
    drawDockHost();

    // 2) floating windows docked into host
    drawViewportWindow();
    tools.onImGui(*this);

    if (ImGui::Begin("Scene")) {
        ImGui::Text("Instances: %d", (int)scene.instances.size());
        ImGui::Text("Draw calls: %d", renderer.lastDrawCalls);
        ImGui::Text("Instances drawn: %d", renderer.lastInstanceCount);
        ImGui::Separator();
        ImGui::Checkbox("Show GAT overlay", &showGatOverlay);
        ImGui::Checkbox("Show texture overlay", &showTextureOverlay);
        ImGui::Text("GAT: %dx%d cell=%.2f", scene.gatW, scene.gatH, scene.cellSize);
        ImGui::Separator();
        if (ImGui::Button("Reseed cubes")) {
            seedScene();
            pushToast("Reseeded");
        }
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
                ImGui::Text("%-20s  %s", name.c_str(),
                            Shortcuts::keyName(code).c_str());
            }
        }
    }
    ImGui::End();

    if (ImGui::Begin("Assets")) {
        ImGui::TextDisabled("Assets/ .fbx .gltf .obj  →  press Import Asset…");
        if (ImGui::Button("Import Asset…")) importModalOpen = true;
        ImGui::Separator();
        std::error_code ec;
        if (std::filesystem::exists("Assets", ec)) {
            for (auto& p : std::filesystem::directory_iterator("Assets", ec)) {
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension().string();
                if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" ||
                    ext == ".glb" || ext == ".dae" || ext == ".ply" || ext == ".stl") {
                    if (ImGui::Selectable(p.path().filename().string().c_str())) {
                        strncpy_s(importPath, sizeof(importPath),
                                  p.path().string().c_str(), _TRUNCATE);
                        importModalOpen = true;
                    }
                }
            }
        } else {
            ImGui::TextDisabled("Assets/ not found next to binary.");
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

    // toasts floating top-right
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

    // status bar last so it sits on top
    tools.drawStatusBar(*this);

    drawImportModal();
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
        ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.20f, nullptr, &main);
        ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.22f, nullptr, &main);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.28f, nullptr, &main);

        ImGui::DockBuilderDockWindow("Tools", left);
        ImGui::DockBuilderDockWindow("Scene", left);
        ImGui::DockBuilderDockWindow("Config", left);
        ImGui::DockBuilderDockWindow("Assets", right);
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
        if (ImGui::MenuItem("Import Asset…")) importModalOpen = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save (S)", "S"))   trySave();
        if (ImGui::MenuItem("Load (F9)"))       tryLoad();
        if (ImGui::MenuItem("Export Map…")) {
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
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("GAT overlay", "G", &showGatOverlay);
        ImGui::MenuItem("Texture overlay", nullptr, &showTextureOverlay);
        if (ImGui::MenuItem("Reset layout")) {
            std::filesystem::remove("imgui.ini");
            dockBuilt = false;
        }
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("| Map-Maker");
    ImGui::EndMenuBar();
}

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
                size_t hash = std::hash<std::string>{}(std::string(importPath));
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

    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;

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
            // GAT hit (plane y = gatOrigin.y)
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
            // Heightmap hit (plane y = hmOrigin.y, first-pass)
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

    if (hovered && !cursorCaptured) {
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

    ImGui::End();
    ImGui::PopStyleVar();
}

void Application::drawGatOverlayInViewport() {
    if (viewportFbo.width() <= 0 || viewportFbo.height() <= 0) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetItemRectMin();
    float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    auto project = [&](const glm::vec3& world, ImVec2& out) -> bool {
        glm::vec4 clip = vp * glm::vec4(world, 1.0f);
        if (clip.w <= 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        out.x = origin.x + (ndc.x * 0.5f + 0.5f) * viewportFbo.width();
        out.y = origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportFbo.height();
        return true;
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
    ImVec2 origin = ImGui::GetItemRectMin();
    float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    auto project = [&](const glm::vec3& world, ImVec2& out) -> bool {
        glm::vec4 clip = vp * glm::vec4(world, 1.0f);
        if (clip.w <= 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        out.x = origin.x + (ndc.x * 0.5f + 0.5f) * viewportFbo.width();
        out.y = origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportFbo.height();
        return true;
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

void Application::handleGlobalKeys() {
    if (shortcuts.justPressed("edit.undo")) {
        if (commands.undo()) pushToast("Undo");
    }
    if (shortcuts.justPressed("edit.redo")) {
        if (commands.redo()) pushToast("Redo");
    }
    if (shortcuts.justPressed("save.quick")) trySave();
    if (shortcuts.justPressed("view.toggleGat")) {
        showGatOverlay = !showGatOverlay;
        pushToast(showGatOverlay ? "GAT overlay ON" : "GAT overlay OFF");
    }
    if (shortcuts.justPressed("view.toggleLandscape")) {
        showTextureOverlay = !showTextureOverlay;
        pushToast(showTextureOverlay ? "Texture overlay ON" : "Texture overlay OFF");
    }
    if (shortcuts.justPressed("tool.cancel") && cursorCaptured) {
        cursorCaptured = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Application::trySave() {
    std::filesystem::create_directories("Save");
    if (SaveIO::save("Save/map.json", scene))
        pushToast("Saved Save/map.json");
    else
        pushToast("Save failed", ToastLevel::Error);
}

void Application::tryLoad() {
    std::string err;
    if (SaveIO::load("Save/map.json", scene, err)) {
        commands.clear();
        renderedHeightmapVersion = -1;   // force rebuild
        pushToast("Loaded Save/map.json");
    } else {
        pushToast("Load failed: " + err, ToastLevel::Error);
    }
}

void Application::shutdown() {
    tools.shutdown();
    renderer.shutdown();
    viewportFbo.destroy();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}