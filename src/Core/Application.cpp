#include "Application.h"
#include "ApplicationInternal.h"
#include "Save/SaveIO.h"
#include "Render/AssetImporter.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {
void glfwErrorCb(int code, const char* desc) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, desc);
}

void glfwScrollCb(GLFWwindow* w, double /*xoff*/, double yoff) {
    auto* app = (Application*)glfwGetWindowUserPointer(w);
    if (app) app->wheelAccum += (float)yoff;
}
} // namespace

glm::mat4 Application::modelMatrix(const Instance& i) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, i.position);
    m = glm::rotate(m, glm::radians(i.rotation.x), { 1,0,0 });
    m = glm::rotate(m, glm::radians(i.rotation.y), { 0,1,0 });
    m = glm::rotate(m, glm::radians(i.rotation.z), { 0,0,1 });
    m = glm::scale(m, i.scale);
    return m;
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

    glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, glfwScrollCb);

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
    // NOTE: NavEnableKeyboard is intentionally NOT set, so Tab does not
    // cycle ImGui widget focus and stays free for the radial menu.

    bool hadIni = std::filesystem::exists("imgui.ini");
    io.IniFilename = "imgui.ini";
    dockBuilt = hadIni;

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

    // Register the unit quad used for GAT / texture overlays. A 1x1 quad in
    // the XZ plane; per-instance model matrix scales it to cellSize.
    {
        Mesh quad = MeshFactory::makePlane(1.0f, 0.0f);
        quad.name = "overlay_quad";
        overlayQuadHash = 0xABCDEF1234ULL;
        renderer.setMesh(overlayQuadHash, std::move(quad));
    }

    viewportFbo.resize(viewportW, viewportH);
    scene.initGat(64, 64);

    // Larger default heightmap, centered on the world origin. 65x65 @ 1.5m
    // gives a ~96-unit-wide terrain (was 33x33 → ~48 units).
    {
        constexpr int kHmW = 65;
        constexpr int kHmH = 65;
        scene.initHeightmap(kHmW, kHmH);
        scene.hmOrigin = glm::vec3(
            -(kHmW - 1) * scene.hmCell * 0.5f, 0.0f,
            -(kHmH - 1) * scene.hmCell * 0.5f);
    }
    seedScene();
    preloadAssets();

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

void Application::preloadAssets() {
    int found = assets.scan("Assets");
    if (found <= 0) {
        pushToast("Assets/ empty or missing - drop models there", ToastLevel::Warning);
        return;
    }

    int loaded = 0, failed = 0;
    for (auto& a : assets.entries()) {
        Mesh m;
        std::string err;
        if (AssetImporter::loadMeshInto(a.fullPath, m, err)) {
            a.loaded = true;
            a.indexCount = (int)m.indexCount;
            renderer.setMesh(a.hash, std::move(m));
            loaded++;
        } else {
            failed++;
            std::fprintf(stderr, "[assets] failed %s: %s\n",
                         a.fullPath.c_str(), err.c_str());
        }
    }
    pushToast("Preloaded " + std::to_string(loaded) + " asset(s)");
    if (failed > 0)
        pushToast(std::to_string(failed) + " asset(s) failed", ToastLevel::Warning);
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

    // Freeze the wheel for the whole frame so nothing downstream can eat it.
    wheelThisFrame = wheelAccum;
    wheelAccum = 0.0f;

    config.poll();
    shortcuts.update(window);
    toasts.update();
    handleGlobalKeys();

    bool toolHasMenu = tools.active() && tools.active()->hasQuickMenu();
    bool wantLookByRMB = shortcuts.isDown("camera.capture") && !toolHasMenu;
    bool wantLookByMMB = shortcuts.isDown("camera.lookMB");

    const bool altDown =
        glfwGetKey(window, GLFW_KEY_LEFT_ALT)  == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    const bool wantLookByAlt = altDown && !ImGui::GetIO().WantTextInput;

    bool wantLook = wantLookByRMB || wantLookByMMB || wantLookByAlt;

    const bool altLookBypassesHover = wantLookByAlt;   // Alt alone ignores hover
    if (wantLook && !cursorCaptured &&
        (viewportHoveredForCamera() || altLookBypassesHover)) {
        cursorCaptured = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (!wantLook && cursorCaptured) {
        cursorCaptured = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    bool blockCam = transformActive;
    camera.update(window, dt, cursorCaptured,
                  !blockCam && shortcuts.isDown("camera.forward"),
                  !blockCam && shortcuts.isDown("camera.back"),
                  !blockCam && shortcuts.isDown("camera.left"),
                  !blockCam && shortcuts.isDown("camera.right"),
                  !blockCam && shortcuts.isDown("camera.up"),
                  !blockCam && shortcuts.isDown("camera.down"),
                  !blockCam && shortcuts.isDown("camera.yawLeft"),
                  !blockCam && shortcuts.isDown("camera.yawRight"),
                  !blockCam && shortcuts.isDown("camera.pitchUp"),
                  !blockCam && shortcuts.isDown("camera.pitchDown"),
                  shortcuts.isDown("camera.fast"));

    if (cursorCaptured && !blockCam) {
        if (wheelThisFrame != 0.0f) {
            camera.position += camera.forward() * (wheelThisFrame * wheelCamStep);
            wheelThisFrame = 0.0f;   // consumed
        }
    }

    // Alt-look fly: LMB pushes the camera forward, RMB pulls it back, both
    // along the view direction. Speed matches the numpad fly speed so the two
    // feel identical; Shift = 4x, matching Camera::update()'s fast modifier.
    if (altDown && cursorCaptured && !blockCam) {
        const bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)
                        == GLFW_PRESS;
        const bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)
                        == GLFW_PRESS;

        float d = 0.0f;
        if (lmb) d += 1.0f;
        if (rmb) d -= 1.0f;

        if (d != 0.0f) {
            const bool fast =
                glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            camera.position += camera.forward()
                            * d * camera.speed * (fast ? 4.0f : 1.0f) * dt;
        }
    }

    tools.update(*this, dt);

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