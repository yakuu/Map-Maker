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

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;

    void update(GLFWwindow* window, float dt, bool captured,
                bool fwdDown, bool backDown,
                bool leftDown, bool rightDown,
                bool upDown, bool downDown,
                bool fast);

    void focusOn(const glm::vec3& target, float distance);
};