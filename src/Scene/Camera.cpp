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
                    bool fast) {
    if (captured) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        static double lastX = mx, lastY = my;
        static bool first = true;
        if (first) { lastX = mx; lastY = my; first = false; }

        float dx = (float)(mx - lastX);
        float dy = (float)(my - lastY);
        lastX = mx; lastY = my;

        yaw   += dx * sensitivity;
        pitch -= dy * sensitivity;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

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

void Camera::focusOn(const glm::vec3& target, float distance) {
    position = target - forward() * distance;
}