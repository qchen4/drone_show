// src/Renderer.cpp
#include "Renderer.h"
#include "Camera.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdio>

#include "common/texture.hpp"
#include "common/objloader.hpp"

// simple shader
// simple shader with optional texture
static const char* kVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 uMVP;
uniform vec3 uColor;

out vec3 vColor;
out vec2 vUV;

void main() {
    vColor = uColor;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(
#version 330 core
in vec3 vColor;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform int uUseTex;

void main() {
    if (uUseTex == 1) {
        FragColor = texture(uTex, vUV);
    } else {
        FragColor = vec4(vColor, 1.0);
    }
}
)";


static unsigned int compile(GLenum type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
    }
    return s;
}

static unsigned int linkProgram(const char* vs, const char* fs) {
    unsigned int v = compile(GL_VERTEX_SHADER, vs);
    unsigned int f = compile(GL_FRAGMENT_SHADER, fs);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    int ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::fprintf(stderr, "Program link error: %s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return prog;
}

static unsigned int createFallbackTexture() {
    // 1x1 white texture so rendering can proceed even if asset is missing.
    unsigned int tex = 0;
    unsigned char pixel[4] = {255, 255, 255, 255};
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

Renderer::Renderer(int width, int height)
    : width_(width), height_(height),
      shader_(0),
      fieldVAO_(0), fieldVBO_(0), fieldEBO_(0),
      fieldUVBO_(0),
      fieldTex_(0),
      cubeVAO_(0), cubeVBO_(0), cubeEBO_(0),
      sphereVAO_(0), sphereVBO_(0), sphereEBO_(0),
      sphereIndexCount_(0),
      ufoVAO_(0), ufoVBO_(0),
      ufoVertexCount_(0),
      ufoTexture_(0)
{
    initGL();
    initShader();

    // Load football field texture (ff.bmp must be in working directory)
    fieldTex_ = loadBMP_custom("ff.bmp");

    initField();
    initCube();
    initSphereWire(32, 16);
    initUFO();
}


Renderer::~Renderer() {
    glDeleteVertexArrays(1, &fieldVAO_);
    glDeleteBuffers(1, &fieldVBO_);
    glDeleteBuffers(1, &fieldEBO_);
    glDeleteBuffers(1, &fieldUVBO_);   // <-- new
    glDeleteTextures(1, &fieldTex_);   // <-- new

    glDeleteVertexArrays(1, &cubeVAO_);
    glDeleteBuffers(1, &cubeVBO_);
    glDeleteBuffers(1, &cubeEBO_);
    glDeleteVertexArrays(1, &sphereVAO_);
    glDeleteBuffers(1, &sphereVBO_);
    glDeleteBuffers(1, &sphereEBO_);
    glDeleteVertexArrays(1, &ufoVAO_);
    glDeleteBuffers(1, &ufoVBO_);
    glDeleteTextures(1, &ufoTexture_);
    glDeleteProgram(shader_);
}


void Renderer::initGL() {
    glEnable(GL_DEPTH_TEST);
}

void Renderer::initShader() {
    shader_ = linkProgram(kVS, kFS);
}

void Renderer::initField() {
    // Approximate football field: 100 yards ~ 91.44 m along y,
    // width ~ 48.8 m along x. We'll center at origin.
    float halfX = 26.0f;
    float halfY = 50.0f;

    float verts[] = {
        -halfX, -halfY, 0.0f,
         halfX, -halfY, 0.0f,
         halfX,  halfY, 0.0f,
        -halfX,  halfY, 0.0f
    };
    unsigned int idx[] = {0,1,2, 2,3,0};

    glGenVertexArrays(1, &fieldVAO_);
    glGenBuffers(1, &fieldVBO_);
    glGenBuffers(1, &fieldEBO_);

    glBindVertexArray(fieldVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, fieldVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fieldEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // --- NEW: UVs for the field ---
    float uvs[] = {
        0.0f, 0.0f,   // bottom-left
        0.0f, 1.0f,   // bottom-right
        1.0f, 1.0f,   // top-right
        1.0f, 0.0f    // top-left
    };

    glGenBuffers(1, &fieldUVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, fieldUVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

    glBindVertexArray(0);
}


void Renderer::initCube() {
    float s = 0.1f; // 20 cm cube
    float v[] = {
        // front
        -s,-s, s,  s,-s, s,  s, s, s,  -s, s, s,
        // back
        -s,-s,-s,  s,-s,-s,  s, s,-s,  -s, s,-s
    };
    unsigned int idx[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,1,5, 5,4,0,
        3,2,6, 6,7,3,
        0,3,7, 7,4,0,
        1,2,6, 6,5,1
    };

    glGenVertexArrays(1, &cubeVAO_);
    glGenBuffers(1, &cubeVBO_);
    glGenBuffers(1, &cubeEBO_);

    glBindVertexArray(cubeVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::initSphereWire(int slices, int stacks) {
    // Simple wireframe sphere using latitude/longitude lines
    std::vector<float> verts;
    std::vector<unsigned int> indices;
    float R = 10.0f; // sphere radius, matches spec

    for (int i=0;i<=stacks;++i) {
        float v = (float)i / stacks;
        float phi = v * M_PI; // 0..pi
        float z = R*std::cos(phi);
        float r = R*std::sin(phi);
        for (int j=0;j<=slices;++j) {
            float u = (float)j / slices;
            float theta = u * 2.0f * M_PI;
            float x = r * std::cos(theta);
            float y = r * std::sin(theta);
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
        }
    }

    for (int i=0;i<stacks;++i) {
        for (int j=0;j<slices;++j) {
            int row1 = i*(slices+1);
            int row2 = (i+1)*(slices+1);
            int a = row1 + j;
            int b = row1 + j+1;
            int c = row2 + j;
            int d = row2 + j+1;
            // use lines
            indices.push_back(a); indices.push_back(b);
            indices.push_back(a); indices.push_back(c);
        }
    }

    sphereIndexCount_ = (int)indices.size();

    glGenVertexArrays(1, &sphereVAO_);
    glGenBuffers(1, &sphereVBO_);
    glGenBuffers(1, &sphereEBO_);

    glBindVertexArray(sphereVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO_);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::initUFO() {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    if (!loadOBJ("OBJ files/duck-float.obj", vertices, uvs, normals)) {
        std::fprintf(stderr, "Failed to load UFO OBJ (duck-float.obj)\n");
        return;
    }

    ufoTexture_ = loadBMP_custom("OBJ files/uvtemplate.bmp");
    if (ufoTexture_ == 0) {
        std::fprintf(stderr, "duck-float texture missing, using fallback white texture\n");
        ufoTexture_ = createFallbackTexture();
    }

    struct Vertex {
        glm::vec3 pos;
        glm::vec2 uv;
    };

    std::vector<Vertex> vtx;
    vtx.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        Vertex vv;
        vv.pos = vertices[i];
        vv.uv  = (i < uvs.size()) ? uvs[i] : glm::vec2(0.0f, 0.0f);
        vtx.push_back(vv);
    }
    ufoVertexCount_ = static_cast<int>(vtx.size());

    glGenVertexArrays(1, &ufoVAO_);
    glGenBuffers(1, &ufoVBO_);

    glBindVertexArray(ufoVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, ufoVBO_);
    glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(Vertex), vtx.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Renderer::resize(int width, int height) {
    width_ = width;
    height_ = height > 0 ? height : 1;
    glViewport(0,0,width_,height_);
}

void Renderer::render(const Camera& camera,
                      const std::vector<UAVRenderInfo>& uavs)
{
    glClearColor(0.1f,0.1f,0.2f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_);
    int locMVP    = glGetUniformLocation(shader_, "uMVP");
    int locColor  = glGetUniformLocation(shader_, "uColor");
    int locUseTex = glGetUniformLocation(shader_, "uUseTex");
    int locTex    = glGetUniformLocation(shader_, "uTex");

    glm::mat4 view = camera.viewMatrix();
    glm::mat4 proj = camera.projMatrix();

    // ---------- Field: textured with ff.bmp ----------
    glBindVertexArray(fieldVAO_);

    // Position attribute (location = 0)
    glBindBuffer(GL_ARRAY_BUFFER, fieldVBO_);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // UV attribute (location = 1)
    glBindBuffer(GL_ARRAY_BUFFER, fieldUVBO_);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);

    glm::mat4 model(1.0f);
    glm::mat4 mvp = proj * view * model;
    glUniformMatrix4fv(locMVP,1,GL_FALSE,glm::value_ptr(mvp));

    // Tell shader to use texture
    glUniform1i(locUseTex, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fieldTex_);
    glUniform1i(locTex, 0);  // texture unit 0

    // uColor is unused when uUseTex == 1, but set it anyway
    glUniform3f(locColor, 0.0f, 0.6f, 0.0f);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // ---------- Sphere: light blue wireframe, no texture ----------
    glBindVertexArray(sphereVAO_);
    glDisableVertexAttribArray(1);  // no UVs

    model = glm::translate(glm::mat4(1.0f), glm::vec3(0,0,50.0f));
    mvp = proj * view * model;
    glUniformMatrix4fv(locMVP,1,GL_FALSE,glm::value_ptr(mvp));

    glUniform1i(locUseTex, 0);
    glUniform3f(locColor, 0.3f, 0.5f, 0.9f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_LINES, sphereIndexCount_, GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // ---------- UAVs: textured UFO model ----------
    glBindVertexArray(ufoVAO_);

    glUniform1i(locUseTex, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ufoTexture_);
    glUniform1i(locTex, 0);
    glUniform3f(locColor, 1.0f, 1.0f, 1.0f);

    for (const auto& u : uavs) {
        model = glm::mat4(1.0f);
        model = glm::translate(model, u.position);
        model = glm::scale(model, glm::vec3(0.02f));
        mvp = proj * view * model;
        glUniformMatrix4fv(locMVP,1,GL_FALSE,glm::value_ptr(mvp));
        glDrawArrays(GL_TRIANGLES, 0, ufoVertexCount_);
    }

    glBindVertexArray(0);
}
