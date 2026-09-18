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

    bool importModalOpen = false;
    char importPath[512] = "Assets/model.obj";

    bool  contentFolderModalOpen = false;
    char  contentFolder[512] = "Assets";

    // Selection outline + transform gizmo
    bool      outlineEnabled = true;
    float     outlineThickness = 0.06f;
    glm::vec4 outlineColor{ 0.25f, 0.75f, 1.0f, 1.0f };

    // Published each frame by TransformTool so the viewport overlay can read it.
    bool      transformActive = false;
    int       transformOp = 0;      // 0 none, 1 grab, 2 rotate, 3 scale
    int       transformAxis = 0;    // 0 none, 1 X, 2 Y, 3 Z
    glm::vec3 transformOrigin{ 0 };

    // Snapping configuration (used by the transform tool while Ctrl is held)
    float gridSnapTranslate = 0.5f;   // world units
    float gridSnapRotate    = 15.0f;  // degrees
    float gridSnapScale     = 0.1f;   // multiplier step

    void pushToast(const std::string& msg, ToastLevel lvl = ToastLevel::Info) {
        toasts.push(msg, lvl);
    }

    // Rebuild the model matrix for an instance (shared by scene + outline paths).
    static glm::mat4 modelMatrix(const Instance& i);

private:
    void frame(float dt);
    void drawScene();
    void drawUi();
    void drawViewportWindow();
    void drawGatOverlayInViewport();
    void drawTextureOverlayInViewport();
    void drawGizmoInViewport();
    void drawDockHost();
    void drawMenuBar();
    void drawImportModal();
    void drawContentFolderModal();
    void handleGlobalKeys();
    void seedScene();
    void rebuildHeightmapMesh();
    void preloadAssets();
    void trySave();
    void tryLoad();
    void focusSelected();
    void duplicateSelected();

    bool viewportHoveredForCamera() const;

    double lastTime = 0.0;
    bool dockBuilt = false;
};