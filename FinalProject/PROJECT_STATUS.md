# Final Project Folder Check

This document summarizes how the current `FinalProject` folder aligns with the course requirements and highlights gaps to address.

## What is implemented
- **Build setup**: `CMakeLists.txt` builds a `FinalProject` executable that pulls in the UAV logic, renderer, camera, and the shared course texture helper, and copies `ff.bmp` plus OBJ assets beside the binary for runtime access.【F:FinalProject/CMakeLists.txt†L5-L30】
- **Threading model**: `main.cpp` spawns 15 `ECE_UAV` objects arranged in a 3×5 grid, starts a worker thread for each, and keeps the main thread for rendering. The render loop polls every 30 ms and swaps velocities for UAVs closer than 1 cm to model elastic collisions.【F:FinalProject/src/main.cpp†L84-L150】
- **UAV kinematics**: Each UAV thread runs at 10 ms steps, waits on the ground for 5 s, ascends toward `(0,0,50)`, then switches to a random tangent flight on a 10 m sphere with speed clamped between 2–10 m/s. The thread stops after 60 s on the sphere.【F:FinalProject/src/ECE_UAV.cpp†L6-L228】
- **Rendering**: The renderer applies the `ff.bmp` field texture, draws a 10 m wireframe sphere centered at `(0,0,50)`, and renders each UAV using a textured OBJ model scaled to a ~20 cm cube footprint. Field, sphere, and UAV draw calls are driven by the camera/view-projection matrices.【F:FinalProject/src/Renderer.cpp†L99-L410】【F:FinalProject/src/Camera.cpp†L4-L22】

## Gaps against the specification
- **Coding standard headers/comments**: Source files lack the required file-level headers and descriptive function/class comments from Appendix A (author, class, last modified, and description blocks, plus per-function documentation).【F:FinalProject/src/main.cpp†L1-L154】【F:FinalProject/src/ECE_UAV.cpp†L1-L240】
- **Flight termination condition**: The simulation ends per-UAV after 60 s on the sphere, but there is no global check that *all* UAVs first come within 10 m of `(0,0,50)` before the 60 s timer, as specified for ending the show.【F:FinalProject/src/ECE_UAV.cpp†L107-L120】
- **Color modulation for ECE6122**: UAV color intensity does not oscillate between half and full brightness at ~0.5 Hz for the ECE6122 requirement; rendering uses a fixed white color for the textured model.【F:FinalProject/src/Renderer.cpp†L395-L411】
- **Initial placement fidelity**: Starting positions are hard-coded metric offsets rather than explicit yard-to-meter conversions of the 0/25/50 yard-line pattern, so the exact field alignment may deviate from the requirement diagram.【F:FinalProject/src/main.cpp†L84-L109】
- **Packaging for submission**: There is no script or note to produce the required `FinalProject.zip` bundle with binaries/video instructions; only the build files and assets are present.【F:FinalProject/CMakeLists.txt†L5-L30】

## Suggested next steps
- Add the mandated file headers and inline documentation to all source/header files in `FinalProject`.
- Introduce a global synchronization/check so the simulation terminates only after all UAVs have come within 10 m of the target and spent the full 60 s on the sphere.
- Apply the ECE6122 color oscillation requirement in the rendering path (e.g., per-UAV color modulation driven by simulation time).
- Revisit initial spawn coordinates to derive exact meter values from the specified yard lines for better authenticity.
- Provide a small packaging script or README note describing how to generate `FinalProject.zip` with assets and, if needed, the demonstration video link.
