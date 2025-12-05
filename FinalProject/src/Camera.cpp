// src/Camera.cpp
#include "Camera.h"

Camera::Camera()
    : position(120.0f, 0.0f, 70.0f),   // farther back + higher
      target(0.0f, 0.0f, 30.0f),         // look slightly upward
      up(0.0f, 0.0f, 1.0f),
      fov(glm::radians(50.0f)),         // slightly wider FOV
      
      aspect(1.0f),
      nearPlane(0.1f),
      farPlane(400.0f)                   // extend to avoid clipping sphere
{}


glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 Camera::projMatrix() const {
    return glm::perspective(fov, aspect, nearPlane, farPlane);
}
