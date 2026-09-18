#include "Shortcuts.h"
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>
#include <fstream>
#include <cstdio>

bool Shortcuts::load(const std::string& path) {
    configPath = path;
    std::ifstream in(path);
    if (!in) return false;
    try {
        nlohmann::json j;
        in >> j;
        for (auto& [k, v] : j.items()) {
            if (v.is_number_integer())
                bindings[k] = v.get<int>();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[shortcuts] load error: %s\n", e.what());
        return false;
    }
    return true;
}

bool Shortcuts::save(const std::string& path) const {
    nlohmann::json j = nlohmann::json::object();
    for (auto& [k, v] : bindings) j[k] = v;
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2);
    return true;
}

void Shortcuts::update(GLFWwindow* window) {
    downPrev = downNow;
    downNow.clear();

    for (auto& [name, code] : bindings) {
        bool pressed = false;

        if (code >= kMouseBase && code < kMouseBase + 8) {
            int mb = code - kMouseBase;
            if (mb >= 0 && mb <= GLFW_MOUSE_BUTTON_LAST)
                pressed = glfwGetMouseButton(window, mb) == GLFW_PRESS;
        } else if (code >= 0 && code <= GLFW_KEY_LAST) {
            int state = glfwGetKey(window, code);
            pressed = (state == GLFW_PRESS || state == GLFW_REPEAT);
        }

        if (pressed) downNow.insert(name);
    }
}

bool Shortcuts::isDown(const std::string& a) const { return downNow.count(a) != 0; }
bool Shortcuts::justPressed(const std::string& a) const {
    return downNow.count(a) && !downPrev.count(a);
}
bool Shortcuts::justReleased(const std::string& a) const {
    return !downNow.count(a) && downPrev.count(a);
}

int Shortcuts::key(const std::string& a) const {
    auto it = bindings.find(a);
    return it == bindings.end() ? -1 : it->second;
}

void Shortcuts::set(const std::string& a, int k) {
    bindings[a] = k;
    if (!configPath.empty()) save(configPath);
}

std::string Shortcuts::keyName(int code) {
    static const char* mouse[] = { "MouseL", "MouseR", "MouseM", "Mouse4", "Mouse5" };
    if (code >= kMouseBase && code < kMouseBase + 5) return mouse[code - kMouseBase];

    const char* n = glfwGetKeyName(code, 0);
    if (n) return std::string(n);

    switch (code) {
        case GLFW_KEY_SPACE:      return "Space";
        case GLFW_KEY_ESCAPE:     return "Esc";
        case GLFW_KEY_ENTER:      return "Enter";
        case GLFW_KEY_TAB:        return "Tab";
        case GLFW_KEY_BACKSPACE:  return "Backspace";
        case GLFW_KEY_DELETE:     return "Del";
        case GLFW_KEY_LEFT:       return "Left";
        case GLFW_KEY_RIGHT:      return "Right";
        case GLFW_KEY_UP:         return "Up";
        case GLFW_KEY_DOWN:       return "Down";
        case GLFW_KEY_LEFT_SHIFT: return "Shift";
        case GLFW_KEY_LEFT_CTRL:  return "Ctrl";
        case GLFW_KEY_LEFT_ALT:   return "Alt";
        default: break;
    }
    if (code >= GLFW_KEY_F1 && code <= GLFW_KEY_F25)
        return "F" + std::to_string(code - GLFW_KEY_F1 + 1);
    return "Key" + std::to_string(code);
}