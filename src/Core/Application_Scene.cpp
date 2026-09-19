#include "Application.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <vector>

void Application::rebuildHeightmapMesh() {
    Scene& s = scene;
    if (s.heightmap.empty()) return;

    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    verts.reserve((size_t)s.hmW * s.hmH);

    auto H = [&](int x, int y) {
        x = std::clamp(x, 0, s.hmW - 1);
        y = std::clamp(y, 0, s.hmH - 1);
        return s.hmAt(x, y);
    };

    for (int y = 0; y < s.hmH; ++y) {
        for (int x = 0; x < s.hmW; ++x) {
            float h = s.hmAt(x, y);
            glm::vec3 p = s.hmOrigin + glm::vec3(x * s.hmCell, h, y * s.hmCell);

            float hl = H(x - 1, y), hr = H(x + 1, y);
            float hd = H(x, y - 1), hu = H(x, y + 1);
            glm::vec3 n = glm::normalize(glm::vec3(
                -(hr - hl) / (2.0f * s.hmCell),
                 1.0f,
                -(hu - hd) / (2.0f * s.hmCell)));

            glm::vec2 uv((float)x / (float)(s.hmW - 1),
                         (float)y / (float)(s.hmH - 1));
            verts.push_back({ p, n, uv });
        }
    }

    for (int y = 0; y < s.hmH - 1; ++y) {
        for (int x = 0; x < s.hmW - 1; ++x) {
            uint32_t i0 = (uint32_t)(y * s.hmW + x);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + s.hmW;
            uint32_t i3 = i2 + 1;
            idx.insert(idx.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }

    Mesh m;
    m.upload(verts, idx);
    m.name = "heightmap";
    renderer.setMesh(heightmapHash, std::move(m));
    renderedHeightmapVersion = s.heightmapVersion;
}

void Application::drawScene() {
    viewportFbo.bind();
    glViewport(0, 0, viewportFbo.width(), viewportFbo.height());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Overlays rely on alpha blending. Normal instances have alpha = 1, so
    // this is a no-op for the main scene.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)viewportFbo.width() / (float)viewportFbo.height();
    glm::mat4 vp = camera.projection(aspect) * camera.view();

    // Overlay lift is chosen so decals stay above the heightmap even at
    // long viewing distances where depth precision is coarse.
    constexpr float kOverlayLift = 0.05f;

    // Nearest-neighbour terrain height for a world X/Z. Used to lift overlays
    // above the heightmap so they don't get buried in a raised cell.
    auto terrainY = [&](float worldX, float worldZ) -> float {
        if (scene.heightmap.empty()) return scene.gatOrigin.y;
        int hx = (int)std::round((worldX - scene.hmOrigin.x) / scene.hmCell);
        int hy = (int)std::round((worldZ - scene.hmOrigin.z) / scene.hmCell);
        if (!scene.hmInBounds(hx, hy)) return scene.gatOrigin.y;
        return std::max(scene.gatOrigin.y, scene.hmAt(hx, hy));
    };

    renderer.begin(vp);

    // Terrain.
    {
        glm::mat4 m(1.0f);
        renderer.submit(heightmapHash, m, glm::vec4(0.35f, 0.55f, 0.3f, 1.0f));
    }

    // Objects.
    for (auto& i : scene.instances)
        renderer.submit(i.meshHash, modelMatrix(i), i.tint);

    // GAT overlay, one quad per non-walkable cell.
    if (showGatOverlay) {
        for (int y = 0; y < scene.gatH; ++y) {
            for (int x = 0; x < scene.gatW; ++x) {
                GatCell c = scene.at(x, y);
                if (c == GatCell::Walkable) continue;

                glm::vec3 center = scene.cellCenter(x, y);
                float gy = terrainY(center.x, center.z) + kOverlayLift;

                glm::mat4 m(1.0f);
                m = glm::translate(m, glm::vec3(center.x, gy, center.z));
                m = glm::scale(m, glm::vec3(scene.cellSize, 1.0f, scene.cellSize));

                glm::vec4 tint = (c == GatCell::NotWalkable)
                    ? glm::vec4(0.95f, 0.28f, 0.28f, 0.55f)
                    : glm::vec4(0.98f, 0.85f, 0.30f, 0.55f);

                renderer.submit(overlayQuadHash, m, tint);
            }
        }
    }

    // Texture-layer overlay, one quad per non-zero cell. Slightly higher
    // than the GAT overlay so it wins when both are visible.
    if (showTextureOverlay) {
        static const glm::vec4 kColors[8] = {
            { 0.00f, 0.00f, 0.00f, 0.00f },
            { 0.47f, 0.70f, 1.00f, 0.50f },
            { 1.00f, 0.70f, 0.47f, 0.50f },
            { 0.70f, 1.00f, 0.47f, 0.50f },
            { 1.00f, 0.47f, 0.70f, 0.50f },
            { 0.47f, 1.00f, 0.80f, 0.50f },
            { 0.80f, 0.47f, 1.00f, 0.50f },
            { 1.00f, 1.00f, 0.47f, 0.50f },
        };

        for (int y = 0; y < scene.gatH; ++y) {
            for (int x = 0; x < scene.gatW; ++x) {
                uint8_t L = scene.textureLayers[(size_t)y * scene.gatW + x];
                if (L == 0) continue;

                glm::vec3 center = scene.cellCenter(x, y);
                float gy = terrainY(center.x, center.z) + kOverlayLift + 0.01f;

                glm::mat4 m(1.0f);
                m = glm::translate(m, glm::vec3(center.x, gy, center.z));
                m = glm::scale(m, glm::vec3(scene.cellSize, 1.0f, scene.cellSize));

                renderer.submit(overlayQuadHash, m, kColors[L % 8]);
            }
        }
    }

    renderer.end();

    // Selection outline.
    if (outlineEnabled && selectedInstance > 0) {
        if (auto* inst = scene.find(selectedInstance)) {
            glm::vec4 col = outlineColor;
            if (transformActive)
                col = glm::vec4(1.0f, 0.85f, 0.25f, 1.0f);
            renderer.drawOutline(inst->meshHash, modelMatrix(*inst),
                                 col, outlineThickness);
        }
    }

    glDisable(GL_BLEND);
    viewportFbo.unbind();
}