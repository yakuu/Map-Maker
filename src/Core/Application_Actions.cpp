#include "Application.h"
#include "Save/SaveIO.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <filesystem>
#include <iterator>
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
        commands.push(std::make_unique<InstancePresenceCommand>(
            &scene, *scene.find(newId), scene.indexOf(newId), true, "Duplicate Instance"));
        pushToast("Duplicated instance " + std::to_string(newId));
    }
}

void Application::handleGlobalKeys() {
    static const char* const toolActions[] = {
        "tool.select", "tool.transform", "tool.gatPaint",
        "tool.landscape", "tool.texturePaint"
    };
    if (!ImGui::GetIO().WantTextInput) {
        for (int i = 0; i < (int)std::size(toolActions); ++i) {
            if (shortcuts.justPressed(toolActions[i])) tools.setActive(i);
        }
    }

    const bool keyboardCaptured = ImGui::GetIO().WantTextInput ||
                                  ImGui::GetIO().WantCaptureKeyboard;
    if (!keyboardCaptured && shortcuts.justPressed("edit.undo")) {
        if (commands.undo()) pushToast("Undo");
    }
    if (!keyboardCaptured && shortcuts.justPressed("edit.redo")) {
        if (commands.redo()) pushToast("Redo");
    }
    if (shortcuts.justPressed("save.quick")) trySave();
    if (shortcuts.justPressed("load.quick")) tryLoad();
    if (shortcuts.justPressed("view.toggleGat")) {
        showGatOverlay = !showGatOverlay;
        pushToast(showGatOverlay ? "GAT overlay ON" : "GAT overlay OFF");
    }
    if (shortcuts.justPressed("view.toggleLandscape")) {
        showTextureOverlay = !showTextureOverlay;
        pushToast(showTextureOverlay ? "Texture overlay ON" : "Texture overlay OFF");
    }
    if (!keyboardCaptured && shortcuts.justPressed("edit.delete") && selectedInstance > 0) {
        if (auto* instance = scene.find(selectedInstance)) {
            const Instance deleted = *instance;
            const size_t index = scene.indexOf(selectedInstance);
            scene.removeInstance(selectedInstance);
            commands.push(std::make_unique<InstancePresenceCommand>(
                &scene, deleted, index, false, "Delete Instance"));
        }
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

    if (!keyboardCaptured && fNow && !prevF && !transformActive && selectedInstance > 0)
        focusSelected();
    if (!keyboardCaptured && ctrl && dNow && !prevD &&
        !transformActive && selectedInstance > 0)
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

void Application::resizeHeightmap(int newW, int newH, bool preserve) {
    if (newW < 2 || newH < 2) return;
    if (newW == scene.hmW && newH == scene.hmH) return;

    HeightmapResizeCommand::State before{
        scene.hmW, scene.hmH, scene.hmCell, scene.hmOrigin, scene.heightmap
    };
    scene.resizeHeightmap(newW, newH, preserve);
    HeightmapResizeCommand::State after{
        scene.hmW, scene.hmH, scene.hmCell, scene.hmOrigin, scene.heightmap
    };
    commands.push(std::make_unique<HeightmapResizeCommand>(
        &scene, std::move(before), std::move(after)));

    // Force a mesh rebuild on the next frame.
    renderedHeightmapVersion = -1;

    pushToast("Heightmap: " + std::to_string(newW) + " x " +
              std::to_string(newH));
}