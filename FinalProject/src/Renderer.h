// src/Renderer.h
#pragma once
#include <vector>
#include <glm/glm.hpp>

class Camera;

struct UAVRenderInfo {
    glm::vec3 position;
};

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    void resize(int width, int height);
    void render(const Camera& camera,
                const std::vector<UAVRenderInfo>& uavs);

private:
    int width_, height_;
    unsigned int shader_;

    unsigned int fieldVAO_, fieldVBO_, fieldEBO_;
    unsigned int fieldUVBO_;      // <-- add this
    unsigned int fieldTex_;       // <-- and this

    unsigned int cubeVAO_, cubeVBO_, cubeEBO_;
    unsigned int sphereVAO_, sphereVBO_, sphereEBO_;
    int sphereIndexCount_;

    unsigned int ufoVAO_, ufoVBO_;
    int ufoVertexCount_;
    unsigned int ufoTexture_;

    void initGL();
    void initShader();
    void initField();
    void initCube();
    void initSphereWire(int slices, int stacks);
    void initUFO();
};
