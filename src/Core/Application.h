#pragma once
#include "Config.h"
#include "Shortcuts.h"
#include "Commands.h"
#include "Toast.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Render/InstanceRenderer.h"
#include "Render/Framebuffer.h"
#include "Render/Mesh.h"
#include "Tools/ToolManager.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <string>
#include <memory>

struct GLFWwindow;

class Application {
public:
    bool init();
    void run();
    void shutdown();

    GLFWwindow* window = nullptr;
    Config config;
    Shortcuts shortcuts;
    ToastQueue toasts;
    Camera camera;
    Scene scene;
    InstanceRenderer renderer;
    Framebuffer viewportFbo;
    CommandStack commands;
    ToolManager tools;

    int viewportW = 1600, viewportH = 900;
    bool cursorCaptured = false;
    bool showGatOverlay = true;
    bool showTextureOverlay = false;
    int selectedInstance = -1;
    size_t cubeHash = 0;

    // Real on-screen rect of the viewport image (set every frame in drawViewportWindow)
    ImVec2 viewportImageMin{ 0, 0 };
    ImVec2 viewportImageSize{ 0, 0 };

    struct HoverCell { bool valid = false; int x = 0; int y = 0; };
    HoverCell hoveredGatCell;

    struct HmHover { bool valid = false; int x = 0; int y = 0; };
    HmHover hoveredHmVertex;

    int    renderedHeightmapVersion = -1;
    size_t heightmapHash = 0xBEEFCAFEull;

    bool importModalOpen = false;
    char importPath[512] = "Assets/model.obj";

    void pushToast(const std::string& msg, ToastLevel lvl = ToastLevel::Info) {
        toasts.push(msg, lvl);
    }

private:
    void frame(float dt);
    void drawScene();
    void drawUi();
    void drawViewportWindow();
    void drawGatOverlayInViewport();
    void drawTextureOverlayInViewport();
    void drawDockHost();
    void drawMenuBar();
    void drawImportModal();
    void handleGlobalKeys();
    void seedScene();
    void rebuildHeightmapMesh();
    void trySave();
    void tryLoad();

    bool viewportHoveredForCamera() const;

    double lastTime = 0.0;
    bool dockBuilt = false;
};