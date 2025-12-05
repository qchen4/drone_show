// tutorial09_several_objects.cpp
// Author: (your name)
// Class: ECE 4122/6122
// Last Date Modified: 2025-10-25
//
// Description:
// Lab 3 solution: loads suzanne.obj, creates 8 textured heads on a ring so
// ears just touch and chins touch the z=0 plane. Adds a green z=0 rectangle,
// disables mouse input, and implements required keyboard camera controls and
// 'L' toggle for diffuse/specular lighting (ambient stays on).

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// Add at top of file with your other includes
#include <glm/gtx/quaternion.hpp>   // if you use the quaternion variant below

using namespace glm;

// Satisfy common/controls.cpp which expects a global named `window`
GLFWwindow* window = nullptr;
static GLFWwindow* gWindow = nullptr; // your local handle


// === common/ helpers from the tutorials ===
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path);
GLuint loadBMP_custom(const char * imagepath);
GLuint loadDDS(const char * imagepath);
bool loadOBJ(
    const char * path,
    std::vector<vec3> & out_vertices,
    std::vector<vec2> & out_uvs,
    std::vector<vec3> & out_normals
);
void indexVBO(
    std::vector<vec3> & in_vertices,
    std::vector<vec2> & in_uvs,
    std::vector<vec3> & in_normals,
    std::vector<unsigned short> & out_indices,
    std::vector<vec3> & out_vertices,
    std::vector<vec2> & out_uvs,
    std::vector<vec3> & out_normals
);


// ======= Lab constants / tweakables =======
static const int   kNHeads = 8;
static const float kHeadScale = 1.0f;   // uniform scale of imported model
static const float kGreenPadMargin = 0.08f; // extra size beyond heads
static const float kSuzanneYawOffsetDeg = -90.0f; // tweak if faces don’t look outward

// ======= Camera (mouse disabled) =======
// Spherical coordinates: radius r, azimuth theta (around +Z), elevation phi (from XY plane)
struct Cam {
    float r   = 6.0f;
    float th  = radians(45.0f);
    float ph = radians(20.0f);
} cam;

static bool gLightOn = true; // diffuse+specular toggle (ambient stays)

// Key handling (GLFW sticky keys used; we’ll poll)
static void handleKeys(float dt) {
    const float aStep = radians(60.0f) * dt; // angular speed
    const float rStep = 2.0f * dt;           // radial speed
    if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS) cam.r = std::max(0.5f, cam.r - rStep);
    if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS) cam.r += rStep;
    if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS) cam.th += aStep;
    if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS) cam.th -= aStep;
    if (glfwGetKey(gWindow, GLFW_KEY_UP) == GLFW_PRESS)   cam.ph = std::min(radians(89.0f), cam.ph + aStep);
    if (glfwGetKey(gWindow, GLFW_KEY_DOWN) == GLFW_PRESS) cam.ph = std::max(radians(-89.0f), cam.ph - aStep);
    static bool lLatch = false;
    if (glfwGetKey(gWindow, GLFW_KEY_L) == GLFW_PRESS) {
        if (!lLatch) { gLightOn = !gLightOn; lLatch = true; }
    } else {
        lLatch = false;
    }
}

// Build camera matrices from spherical coords
static void computeViewProj(mat4& V, mat4& P, int width, int height) {
    const float cx = cam.r * cosf(cam.ph) * cosf(cam.th);
    const float cy = cam.r * cosf(cam.ph) * sinf(cam.th);
    const float cz = cam.r * sinf(cam.ph);
    V = lookAt(vec3(cx, cy, cz), vec3(0,0,0), vec3(0,0,1));
    P = perspective(radians(45.0f), width / float(height), 0.1f, 100.0f);
}

// Compute bbox for sizing/origin adjustments
struct BBox {
    vec3 mn{ 1e9f }, mx{ -1e9f };
    void expand(const vec3& v){ mn = min(mn, v); mx = max(mx, v); }
    vec3 size() const { return mx - mn; }
    vec3 center() const { return 0.5f*(mn+mx); }
};

int main() {
    // GLFW init
    if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return -1; }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    gWindow = glfwCreateWindow(1024, 768, "Lab3 - Several Objects", NULL, NULL);
    if (gWindow == nullptr) {
        fprintf(stderr, "Failed to open GLFW window.\n");
        glfwTerminate(); return -1;
    }
    glfwMakeContextCurrent(gWindow);

    // Disable mouse look and show cursor (as requested)
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetInputMode(gWindow, GLFW_STICKY_KEYS, GL_TRUE);

    // GLEW
    glewExperimental = true;
    if (glewInit() != GLEW_OK) { fprintf(stderr, "Failed to initialize GLEW\n"); return -1; }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.f, 0.f, 0.15f, 1.f);

    // VAO
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    // Shaders (use your existing tutorial shaders in common/)
    GLuint programID = LoadShaders("StandardShading.vertexshader",
                                   "StandardShading.fragmentshader");
    GLint MatrixID   = glGetUniformLocation(programID, "MVP");
    GLint ViewMatrixID = glGetUniformLocation(programID, "V");
    GLint ModelMatrixID = glGetUniformLocation(programID, "M");
    GLint LightID    = glGetUniformLocation(programID, "LightPosition_worldspace");
    GLint DiffuseID  = glGetUniformLocation(programID, "MaterialDiffuseColor");
    GLint AmbientID  = glGetUniformLocation(programID, "MaterialAmbientColor");
    GLint SpecularID = glGetUniformLocation(programID, "MaterialSpecularColor");

    // Texture
    // GLuint Texture = loadBMP_custom("uvtemplate.bmp");
    GLuint Texture = loadDDS("uvmap.DDS");

    GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");

    // Load OBJ and index VBO
    std::vector<vec3> vertices, normals, indexed_vertices, indexed_normals;
    std::vector<vec2> uvs, indexed_uvs;
    std::vector<unsigned short> indices;

    if (!loadOBJ("suzanne.obj", vertices, uvs, normals)) {
        fprintf(stderr, "Failed to load suzanne.obj\n"); return -1;
    }
    // Compute bbox
    BBox box;
    for (auto &v : vertices) box.expand(v);
    const vec3 size = box.size();
    const float headWidth  = size.x; // assume X is ear-to-ear width in provided OBJ
    const float headDepth  = size.y; // forward direction ~ +Y (common in some exports)
    const float headHeight = size.z; // Z up

    // Center and scale vertices
    const vec3 center = box.center();
    for (auto &v : vertices) {
        v = (v - center) * kHeadScale;
    }

    indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs, indexed_normals);

    // Buffers for Suzanne
    GLuint vertexbuffer, uvbuffer, normalbuffer, elementbuffer;
    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(vec3), &indexed_vertices[0], GL_STATIC_DRAW);

    glGenBuffers(1, &uvbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(vec2), &indexed_uvs[0], GL_STATIC_DRAW);

    glGenBuffers(1, &normalbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
    glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(vec3), &indexed_normals[0], GL_STATIC_DRAW);

    glGenBuffers(1, &elementbuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

    // Determine ring radius so adjacent heads just touch at ears.
    // chord length between adjacent centers = headWidth
    const float R = (headWidth * kHeadScale) / (2.0f * sin(float(M_PI) / float(kNHeads)));

    // Place chins on z=0: move model so its min z is at 0 after scaling.
    // Since we centered vertices, min z ≈ -headHeight/2; raise by +headHeight/2.
    const float chinLift = (headHeight * kHeadScale) * 0.5f;

    // Prepare a green rectangle on z=0 that extends past the heads.
    // Make it a disk-aligned rectangle sized by (R + front margin).
    const float front = (headDepth * kHeadScale) * 0.5f + kGreenPadMargin * (headDepth + headWidth);

    struct Plane {
        GLuint vao=0,vb=0,eb=0;
        GLsizei idxCount=0;
    } plane;

    {
        // Simple rectangle centered at origin on z=0, spanning slightly beyond R+front.
        const float pad = R + front;
        std::vector<vec3> ppos = {
            {-pad, -pad, 0.0f}, { pad, -pad, 0.0f},
            { pad,  pad, 0.0f}, {-pad,  pad, 0.0f}
        };
        std::vector<vec2> puv  = { {0,0},{1,0},{1,1},{0,1} };
        std::vector<unsigned short> pidx = { 0,1,2,  2,3,0 };

        glGenVertexArrays(1, &plane.vao);
        glBindVertexArray(plane.vao);

        glGenBuffers(1, &plane.vb);
        glBindBuffer(GL_ARRAY_BUFFER, plane.vb);
        struct PNT { vec3 p; vec2 t; };
        std::vector<PNT> ppack(4);
        for (int i=0;i<4;++i){ ppack[i].p=ppos[i]; ppack[i].t=puv[i]; }
        glBufferData(GL_ARRAY_BUFFER, ppack.size()*sizeof(PNT), ppack.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &plane.eb);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, plane.eb);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, pidx.size()*sizeof(unsigned short), pidx.data(), GL_STATIC_DRAW);

        plane.idxCount = (GLsizei)pidx.size();
        glBindVertexArray(0);
    }

    // Time
    double lastTime = glfwGetTime();

    do {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        handleKeys(dt);
        if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;

        // Camera
        int width=1024, height=768;
        glfwGetFramebufferSize(gWindow, &width, &height);
        glViewport(0,0,width,height);

        mat4 V, P;
        computeViewProj(V, P, width, height);

        // Use shader
        glUseProgram(programID);

        // Light
        const vec3 lightPos = vec3(6,6,6);
        glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

        // Material colors with toggle: ambient always on, diffuse/specular can be zeroed.
        const vec3 ambient (0.15f, 0.15f, 0.15f);
        const vec3 diffuse = gLightOn ? vec3(0.8f,0.8f,0.8f) : vec3(0,0,0);
        const vec3 specular= gLightOn ? vec3(0.4f,0.4f,0.4f) : vec3(0,0,0);
        glUniform3f(AmbientID,  ambient.x,  ambient.y,  ambient.z);
        glUniform3f(DiffuseID,  diffuse.x,  diffuse.y,  diffuse.z);
        glUniform3f(SpecularID, specular.x, specular.y, specular.z);

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glUniform1i(TextureID, 0);

        // Bind Suzanne buffers
        glBindVertexArray(VertexArrayID);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

        for (int i = 0; i < kNHeads; ++i) {
            float ang = (2.0f * float(M_PI) * i) / float(kNHeads);

            // Position on ring, lifted so chin sits at z=0
            glm::vec3 pos( R * cosf(ang), R * sinf(ang), chinLift );

            // --- Option A: build an orthonormal basis so +Z_local -> outward, Up stays +Z_world ---
            glm::vec3 outward = glm::normalize(glm::vec3(cosf(ang), sinf(ang), 0.0f)); // radial in XY
            glm::vec3 up      = glm::vec3(0,0,1);                                      // keep world-up
            glm::vec3 right   = glm::normalize(glm::cross(up, outward));
            glm::vec3 up2     = glm::cross(outward, right); // re-orthogonalize

            // Column-major: columns are the basis vectors in world space
            glm::mat4 Rm(1.0f);
            Rm[0] = glm::vec4(right,   0.0f);
            Rm[1] = glm::vec4(up2,     0.0f);
            Rm[2] = glm::vec4(outward, 0.0f);  // local +Z points outward

            glm::mat4 M(1.0f);
            M = glm::translate(M, pos) * Rm;

            // (optional) small fixed tweak if your OBJ’s axes differ
            // M = M * glm::rotate(glm::mat4(1.0f), glm::radians(kSuzanneYawOffsetDeg), glm::vec3(0,0,1));

            glm::mat4 MVP = P * V * M;
            glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &M[0][0]);
            glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &V[0][0]);

            glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_SHORT, (void*)0);
        }


        // --- Draw the green z=0 rectangle last (uses same shader) ---
        // Give it a solid green by modulating the texture coords (set diffuse to green temporarily)
        vec3 savedDiffuse = gLightOn ? vec3(0.8f) : vec3(0.f);
        glUniform3f(DiffuseID, 0.0f, gLightOn ? 0.6f : 0.0f, 0.0f);

        mat4 Mp = mat4(1.0f); // z=0
        mat4 MVPp = P * V * Mp;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVPp[0][0]);
        glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &Mp[0][0]);

        glBindVertexArray(plane.vao);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, plane.vb);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*5, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*5, (void*)(sizeof(float)*3));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, plane.eb);
        glDrawElements(GL_TRIANGLES, plane.idxCount, GL_UNSIGNED_SHORT, (void*)0);

        // restore
        glUniform3f(DiffuseID, savedDiffuse.x, savedDiffuse.y, savedDiffuse.z);

        // --- end draw ---
        glfwSwapBuffers(gWindow);
        glfwPollEvents();

    } while (glfwWindowShouldClose(gWindow) == 0);

    // Cleanup
    glDeleteBuffers(1, &vertexbuffer);
    glDeleteBuffers(1, &uvbuffer);
    glDeleteBuffers(1, &normalbuffer);
    glDeleteBuffers(1, &elementbuffer);
    glDeleteTextures(1, &Texture);
    glDeleteVertexArrays(1, &VertexArrayID);
    glDeleteProgram(programID);

    glfwTerminate();
    return 0;
}

// === Minimal stubs (link with your real implementations in common/) ===
// If your project already links these, remove these declarations above instead.
