#pragma once
#ifndef MAPMAKER_CORE_APPLICATION_H
#define MAPMAKER_CORE_APPLICATION_H

#include "Config.h"
#include "Shortcuts.h"
#include "Commands.h"
#include "Toast.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Render/InstanceRenderer.h"
#include "Render/Framebuffer.h"
#include "Render/Mesh.h"
#include "Render/AssetRegistry.h"
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
    AssetRegistry assets;

    int viewportW = 1600, viewportH = 900;
    bool cursorCaptured = false;
    bool showGatOverlay = true;
    bool showTextureOverlay = false;
    int selectedInstance = -1;
    size_t cubeHash = 0;

    ImVec2 viewportImageMin{ 0, 0 };
    ImVec2 viewportImageSize{ 0, 0 };

    struct HoverCell { bool valid = false; int x = 0; int y = 0; };
    HoverCell hoveredGatCell;

    struct HmHover { bool valid = false; int x = 0; int y = 0; };
    HmHover hoveredHmVertex;

    int    renderedHeightmapVersion = -1;
    size_t heightmapHash = 0xBEEFCAFEull;

    // Overlay unit-quad registered with the renderer once in init().
    size_t overlayQuadHash = 0;

    bool importModalOpen = false;
    char importPath[512] = "Assets/model.obj";

    bool  contentFolderModalOpen = false;
    char  contentFolder[512] = "Assets";

    std::string contentFilterExt;

    bool      outlineEnabled = true;
    float     outlineThickness = 0.06f;
    glm::vec4 outlineColor{ 0.25f, 0.75f, 1.0f, 1.0f };

    bool      transformActive = false;
    int       transformOp = 0;
    int       transformAxis = 0;
    glm::vec3 transformOrigin{ 0 };

    float gridSnapTranslate = 0.5f;
    float gridSnapRotate    = 15.0f;
    float gridSnapScale     = 0.1f;

    float wheelCamStep = 1.25f;

    // Raw wheel collected via GLFW callback; consumed once per frame.
    float wheelAccum     = 0.0f;
    float wheelThisFrame = 0.0f;

    int       gizmoDragAxis     = 0;
    int       gizmoHoverAxis    = 0;
    bool      gizmoDragScaling  = false;
    int       gizmoDragTargetId = -1;
    glm::vec3 gizmoDragStartPos{ 0 };
    glm::vec3 gizmoDragStartScale{ 1 };
    glm::vec2 gizmoDragClickMouse{ 0 };
    glm::vec3 gizmoDragAxisDir{ 1, 0, 0 };
    glm::vec2 gizmoDragAxisScreenDir{ 1, 0 };
    float     gizmoDragScreenLen = 1.0f;
    float     gizmoDragWorldLen  = 1.0f;

    void pushToast(const std::string& msg, ToastLevel lvl = ToastLevel::Info) {
        toasts.push(msg, lvl);
    }

    static glm::mat4 modelMatrix(const Instance& i);
    void resizeHeightmap(int newW, int newH, bool preserve = true);

private:
    void frame(float dt);
    void drawScene();
    void drawUi();
    void drawViewportWindow();
    void drawBrushPreview();
    void drawHeightmapBrushPreview();
    void drawGizmoInViewport();
    void drawDockHost();
    void drawMenuBar();
    void drawImportModal();
    void drawContentFolderModal();
    void drawContentPanel();
    void drawSceneListPanel();
    void handleGlobalKeys();
    void seedScene();
    void rebuildHeightmapMesh();
    void preloadAssets();
    void trySave();
    void tryLoad();
    void focusSelected();
    void duplicateSelected();

    int  computeGizmoHitAxis(const ImVec2& mouse);
    bool handleGizmoInput(const ImVec2& mouse, bool clicked, bool down, bool released);

    bool viewportHoveredForCamera() const;

    double lastTime = 0.0;
    bool dockBuilt = false;
};

#endif // MAPMAKER_CORE_APPLICATION_H