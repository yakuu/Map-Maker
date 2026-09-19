#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>
#include <GLFW/glfw3.h>
#include <cmath>

glm::vec3 Camera::forward() const {
    float cy = std::cos(glm::radians(yaw));
    float sy = std::sin(glm::radians(yaw));
    float cp = std::cos(glm::radians(pitch));
    float sp = std::sin(glm::radians(pitch));
    return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0)));
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projection(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, nearP, farP);
}

void Camera::update(GLFWwindow* window, float dt, bool captured,
                    bool fwdDown, bool backDown,
                    bool leftDown, bool rightDown,
                    bool upDown, bool downDown,
                    bool yawLeftDown, bool yawRightDown,
                    bool pitchUpDown, bool pitchDownDown,
                    bool fast) {
    // ---- Mouse-look while the cursor is captured ----
    if (captured && !cursorWasCaptured) {
        // Just entered capture: sync lastCursor to the current cursor so
        // this frame's delta is zero. Prevents the "snap" on Alt-down.
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        lastCursorX = mx;
        lastCursorY = my;
    }
    cursorWasCaptured = captured;

    if (captured) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        const float dx = (float)(mx - lastCursorX);
        const float dy = (float)(my - lastCursorY);
        lastCursorX = mx;
        lastCursorY = my;

        yaw   += dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

    // ---- Keyboard look (numpad 7/9 yaw, 1/3 pitch) ----
    float rot = rotationSpeed * dt;
    if (yawLeftDown)   yaw   -= rot;
    if (yawRightDown)  yaw   += rot;
    if (pitchUpDown)   pitch += rot;
    if (pitchDownDown) pitch -= rot;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // ---- Movement ----
    glm::vec3 dir(0.0f);
    glm::vec3 f = forward();
    glm::vec3 r = right();

    if (fwdDown)   dir += f;
    if (backDown)  dir -= f;
    if (rightDown) dir += r;
    if (leftDown)  dir -= r;
    if (upDown)    dir += glm::vec3(0, 1, 0);
    if (downDown)  dir -= glm::vec3(0, 1, 0);

    if (glm::length(dir) > 0.0001f)
        position += glm::normalize(dir) * speed * (fast ? 4.0f : 1.0f) * dt;
}

void Camera::reset() {
    position = defaultPosition;
    yaw      = defaultYaw;
    pitch    = defaultPitch;
}

void Camera::focusOn(const glm::vec3& target, float distance) {
    position = target - forward() * distance;
}