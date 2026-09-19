#include "Application.h"
#include "Save/SaveIO.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <filesystem>
#include <string>

void Application::focusSelected() {
    if (auto* inst = scene.find(selectedInstance)) {
        float s = std::max({ inst->scale.x, inst->scale.y, inst->scale.z });
        float dist = std::max(3.0f, s * 5.0f);
        camera.focusOn(inst->position, dist);
        pushToast("Focused instance " + std::to_string(inst->id));
    }
}

void Application::duplicateSelected() {
    if (auto* inst = scene.find(selectedInstance)) {
        Instance copy = *inst;
        copy.position += glm::vec3(0.5f, 0.0f, 0.0f);
        int newId = scene.addInstance(copy);
        selectedInstance = newId;
        pushToast("Duplicated instance " + std::to_string(newId));
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
    if (shortcuts.justPressed("edit.delete") && selectedInstance > 0) {
        scene.removeInstance(selectedInstance);
        pushToast("Deleted instance");
        selectedInstance = -1;
    }
    if (shortcuts.justPressed("tool.cancel") && cursorCaptured) {
        cursorCaptured = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    if (shortcuts.justPressed("camera.reset")) {
        camera.reset();
        pushToast("View reset");
    }

    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)  == GLFW_PRESS
             || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    static bool prevF = false, prevD = false;
    bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    bool dNow = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    if (fNow && !prevF && !transformActive && selectedInstance > 0)
        focusSelected();
    if (ctrl && dNow && !prevD && !transformActive && selectedInstance > 0)
        duplicateSelected();

    prevF = fNow;
    prevD = dNow;
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
        renderedHeightmapVersion = -1;
        pushToast("Loaded Save/map.json");
    } else {
        pushToast("Load failed: " + err, ToastLevel::Error);
    }
}