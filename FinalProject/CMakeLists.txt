cmake_minimum_required(VERSION 3.0)

project(FinalProject)

# Executable with your sources
add_executable(FinalProject
    src/main.cpp
    src/ECE_UAV.cpp
    src/Camera.cpp
    src/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/common/texture.cpp
)

# Use the course’s OpenGL stack (GLEW + GLFW + OpenGL)
# ALL_LIBS is defined in the top-level CMakeLists.txt you showed
target_link_libraries(FinalProject
    ${ALL_LIBS}
)

# Make sure we can see common/ and external headers
target_include_directories(FinalProject PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/external/glm-0.9.7.1
    ${CMAKE_SOURCE_DIR}/external/glew-1.13.0/include
    ${CMAKE_SOURCE_DIR}/external/glfw-3.1.2/include
)

# Copy assets into the build dir next to the executable
file(COPY assets/ff.bmp DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY assets/OBJ\ files DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
