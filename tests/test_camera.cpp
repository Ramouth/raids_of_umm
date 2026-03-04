// test_camera.cpp
// Uncomment each SUITE block and add Camera2D.cpp to the test target
// once src/render/Camera2D.h/cpp has been implemented.
//
// Required interface assumed:
//   Camera2D(int vpW, int vpH)
//   Camera2D::pan(float dx, float dz)
//   Camera2D::setZoom(float z)
//   Camera2D::zoom() -> float
//   Camera2D::position() -> glm::vec2   (world XZ of camera centre)
//   Camera2D::viewMatrix()  -> glm::mat4
//   Camera2D::projMatrix()  -> glm::mat4
//   Camera2D::screenToWorld(int px, int py) -> glm::vec2
//
// GLM is header-only so no extra link dependency.

#include "test_runner.h"

#ifdef CAMERA2D_IMPL   // flip this define once Camera2D exists

#include "render/Camera2D.h"
#include <glm/glm.hpp>
#include <cmath>

SUITE("Camera2D — default position is origin") {
    Camera2D cam(1280, 720);
    auto pos = cam.position();
    CHECK_NEAR(pos.x, 0.0f, 1e-4f);
    CHECK_NEAR(pos.y, 0.0f, 1e-4f);
}

SUITE("Camera2D — pan moves position") {
    Camera2D cam(1280, 720);
    cam.pan(3.0f, 2.0f);
    auto pos = cam.position();
    CHECK_NEAR(pos.x, 3.0f, 1e-4f);
    CHECK_NEAR(pos.y, 2.0f, 1e-4f);
}

SUITE("Camera2D — pan is additive") {
    Camera2D cam(1280, 720);
    cam.pan(1.0f, 0.0f);
    cam.pan(2.0f, 0.0f);
    CHECK_NEAR(cam.position().x, 3.0f, 1e-4f);
}

SUITE("Camera2D — zoom clamps to min") {
    Camera2D cam(1280, 720);
    cam.setZoom(-999.0f);           // below minimum
    CHECK(cam.zoom() > 0.0f);       // must stay positive
}

SUITE("Camera2D — zoom clamps to max") {
    Camera2D cam(1280, 720);
    cam.setZoom(999.0f);
    float z = cam.zoom();
    CHECK(z < 999.0f);              // must be clamped
}

SUITE("Camera2D — screenToWorld: centre of screen maps to camera position") {
    Camera2D cam(1280, 720);
    cam.pan(5.0f, 3.0f);   // camera centred at (5, 3)

    // Centre pixel should map to world (5, 3) within floating-point tolerance
    glm::vec2 world = cam.screenToWorld(640, 360);
    CHECK_NEAR(world.x, 5.0f, 0.05f);
    CHECK_NEAR(world.y, 3.0f, 0.05f);
}

SUITE("Camera2D — screenToWorld: consistent with viewProj") {
    // screenToWorld(0,0) and screenToWorld(1280,720) must be on opposite
    // sides of the camera centre — i.e. the projection is centred.
    Camera2D cam(1280, 720);
    glm::vec2 topLeft     = cam.screenToWorld(0,    0);
    glm::vec2 bottomRight = cam.screenToWorld(1280, 720);
    glm::vec2 centre      = cam.screenToWorld(640,  360);

    // centre is the midpoint of the diagonal
    CHECK_NEAR(centre.x, (topLeft.x + bottomRight.x) * 0.5f, 0.05f);
    CHECK_NEAR(centre.y, (topLeft.y + bottomRight.y) * 0.5f, 0.05f);
}

SUITE("Camera2D — viewport resize updates projection") {
    Camera2D cam(800, 600);
    glm::vec2 w1 = cam.screenToWorld(400, 300);  // centre before resize

    cam.resize(1600, 900);
    glm::vec2 w2 = cam.screenToWorld(800, 450);  // centre after resize

    // Both should hit world origin (camera hasn't moved)
    CHECK_NEAR(w1.x, 0.0f, 0.05f);
    CHECK_NEAR(w2.x, 0.0f, 0.05f);
}

SUITE("Camera2D — setPosition moves to absolute location") {
    Camera2D cam(1280, 720);
    cam.setPosition({10.0f, -5.0f});
    auto pos = cam.position();
    CHECK_NEAR(pos.x, 10.0f, 1e-4f);
    CHECK_NEAR(pos.y, -5.0f, 1e-4f);
}

SUITE("Camera2D — adjustZoom changes zoom level") {
    Camera2D cam(1280, 720);
    float initialZoom = cam.zoom();
    cam.adjustZoom(2.0f);
    CHECK(cam.zoom() > initialZoom);

    cam.adjustZoom(-1.0f);
    CHECK(cam.zoom() < initialZoom + 2.0f);
}

SUITE("Camera2D — adjustZoom clamps at limits") {
    Camera2D cam(1280, 720);
    cam.adjustZoom(999.0f);
    CHECK(cam.zoom() <= Camera2D::MAX_ZOOM);

    cam.setZoom(5.0f);
    cam.adjustZoom(-999.0f);
    CHECK(cam.zoom() >= Camera2D::MIN_ZOOM);
}

SUITE("Camera2D — vpWidth and vpHeight return dimensions") {
    Camera2D cam(1024, 768);
    CHECK_EQ(cam.vpWidth(), 1024);
    CHECK_EQ(cam.vpHeight(), 768);

    cam.resize(1920, 1080);
    CHECK_EQ(cam.vpWidth(), 1920);
    CHECK_EQ(cam.vpHeight(), 1080);
}

SUITE("Camera2D — viewMatrix translates and scales") {
    Camera2D cam(1280, 720);
    cam.setPosition({5.0f, 3.0f});
    cam.setZoom(10.0f);

    glm::mat4 view = cam.viewMatrix();

    CHECK(view != glm::mat4(1.0f));
}

SUITE("Camera2D — projMatrix creates orthographic projection") {
    Camera2D cam(1280, 720);
    glm::mat4 proj = cam.projMatrix();

    CHECK(proj[3][3] == 1.0f);
}

SUITE("Camera2D — viewProjMatrix combines view and projection") {
    Camera2D cam(1280, 720);
    glm::mat4 viewProj = cam.viewProjMatrix();
    glm::mat4 view = cam.viewMatrix();
    glm::mat4 proj = cam.projMatrix();

    glm::mat4 combined = proj * view;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK_NEAR(viewProj[i][j], combined[i][j], 1e-5f);
        }
    }
}

#endif // CAMERA2D_IMPL
