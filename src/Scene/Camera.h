#pragma once
#include <glm/glm.hpp>

struct GLFWwindow;

class Camera {
public:
    glm::vec3 position{ 0.0f, 4.0f, 10.0f };
    float yaw   = -90.0f;
    float pitch = -20.0f;
    float fov   = 60.0f;
    float nearP = 0.1f;
    float farP  = 1000.0f;
    float speed = 12.0f;
    float sensitivity = 0.12f;
    float rotationSpeed = 90.0f;   // degrees per second for keyboard look
    // Cursor tracking for mouse-look. Reset on each capture start so the
    // first frame after Alt-down doesn't produce a jump.
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;
    bool   cursorWasCaptured = false;
    // Reset pose
    glm::vec3 defaultPosition{ 0.0f, 4.0f, 10.0f };
    float     defaultYaw   = -90.0f;
    float     defaultPitch = -20.0f;

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;

    void update(GLFWwindow* window, float dt, bool captured,
                bool fwdDown, bool backDown,
                bool leftDown, bool rightDown,
                bool upDown, bool downDown,
                bool yawLeftDown, bool yawRightDown,
                bool pitchUpDown, bool pitchDownDown,
                bool fast);

    void reset();
    void focusOn(const glm::vec3& target, float distance);
};