// src/main.cpp
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <atomic>
#include <mutex>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "ECE_UAV.h"
#include "Camera.h"
#include "Renderer.h"

static std::atomic<bool> gRunning(true);

// simple collision: swap velocities if closer than 0.01 m
void checkCollisions(std::vector<ECE_UAV*>& uavs) {
    const double minDist = 0.01; // 1 cm
    for (size_t i=0;i<uavs.size();++i) {
        for (size_t j=i+1;j<uavs.size();++j) {
            // lock both UAVs in a defined order to avoid deadlock
            ECE_UAV* a = uavs[i];
            ECE_UAV* b = uavs[j];
            std::scoped_lock<std::mutex,std::mutex> lock(a->mtx, b->mtx);
            Vec3 pa = a->pos;
            Vec3 pb = b->pos;
            Vec3 diff = pa - pb;
            if (diff.mag() < minDist) {
                std::swap(a->vel, b->vel);
            }
        }
    }
}

static void framebuffer_size_callback(GLFWwindow* window,int w,int h) {
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer) {
        renderer->resize(w,h);
    }
}

int main() {
    std::cout << "ECE 4122/6122 Final Project - Buzzy Bowl Simulation\n";

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    int width = 400;
    int height = 400;
    GLFWwindow* window = glfwCreateWindow(width,height,"Buzzy Bowl",nullptr,nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }

    Renderer renderer(width,height);
    glfwSetWindowUserPointer(window, &renderer);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    Camera camera;
    camera.aspect = static_cast<float>(width)/height;

    using clock = std::chrono::steady_clock;
    auto startTime = clock::now();

    // Create 15 UAVs at 0,25,50,25,0 yard-lines
    // Convert yards -> meters: 1 yard ≈ 0.9144 m
    const double yard = 0.9144;
    std::vector<double> yardlines = {0,25,50,25,0};
    std::vector<ECE_UAV*> uavs;
    uavs.reserve(15);

    // 3 columns across field (left, center, right)
    const float xCols[3] = { -18.0f, 0.0f, 18.0f };

    // 5 yard-lines along field: 0,25,50,25,0
    const float yLines[5] = { -40.0f, -25.0f, -5.0f, 15.0f, 30.0f };

    int id = 0;
    for (int iy = 0; iy < 5; ++iy) {
        for (int ix = 0; ix < 3; ++ix) {
            float x = xCols[ix];
            float y = yLines[iy];
            float z = 0.0f;          // on the ground

             Vec3 initialPos(x, y, z);
            auto* u = new ECE_UAV(id++, initialPos, &gRunning, startTime);
            uavs.push_back(u);
        }
    }
    std::cerr << "Created " << uavs.size() << " UAVs\n";  // should print 15

    // start each UAV's worker thread
    for (auto* u : uavs) {
        u->start();
    }

    // main render loop, polls every 30 ms
    auto nextPoll = clock::now();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // collision check (operate on shared data with locks)
        checkCollisions(uavs);

        // gather positions for rendering
        std::vector<UAVRenderInfo> renderInfos;
        renderInfos.reserve(uavs.size());
        for (auto* u : uavs) {
            Vec3 p = u->getPositionSafe();
            renderInfos.push_back(UAVRenderInfo{
                glm::vec3((float)p.x,(float)p.y,(float)p.z)
            });
        }

        renderer.render(camera, renderInfos);
        glfwSwapBuffers(window);

        // 30 ms poll interval for main thread
        nextPoll += std::chrono::milliseconds(30);
        std::this_thread::sleep_until(nextPoll);
    }

    // shutdown
    gRunning.store(false);
    for (auto* u : uavs) {
        u->join();
        delete u;
    }

    glfwTerminate();
    return 0;
}
