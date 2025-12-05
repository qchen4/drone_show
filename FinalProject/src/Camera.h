// src/Camera.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;

    float fov;
    float aspect;
    float nearPlane;
    float farPlane;

    Camera();

    glm::mat4 viewMatrix() const;
    glm::mat4 projMatrix() const;
};
