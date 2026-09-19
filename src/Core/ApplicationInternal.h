#pragma once

// Internal helpers shared across the split Application_*.cpp translation units.
// Not part of the public API - do not include from outside src/Core.

#include "Scene/Scene.h"

#include <glm/glm.hpp>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>

namespace AppInternal {

// March a ray against the heightmap; falls back to the y=0 plane.
inline bool raycastGround(Scene& s, glm::vec3 origin, glm::vec3 dir,
                          glm::vec3& hitOut) {
    if (s.heightmap.empty()) {
        if (std::abs(dir.y) > 1e-4f) {
            float t = -origin.y / dir.y;
            if (t > 0) { hitOut = origin + dir * t; return true; }
        }
        return false;
    }

    glm::vec3 prev(0);
    float prevDiff = 0.0f;
    bool  havePrev = false;

    for (float t = 0.0f; t < 500.0f; t += 0.25f) {
        glm::vec3 p = origin + dir * t;
        float hx = (p.x - s.hmOrigin.x) / s.hmCell;
        float hy = (p.z - s.hmOrigin.z) / s.hmCell;
        int ix = (int)std::round(hx);
        int iy = (int)std::round(hy);
        if (!s.hmInBounds(ix, iy)) continue;

        float groundY = s.hmAt(ix, iy);
        float diff = p.y - groundY;

        if (havePrev && prevDiff > 0.0f && diff <= 0.0f) {
            float u = prevDiff / (prevDiff - diff + 1e-6f);
            hitOut = glm::mix(prev, p, u);
            return true;
        }
        prev = p; prevDiff = diff; havePrev = true;
    }

    if (std::abs(dir.y) > 1e-4f) {
        float t = -origin.y / dir.y;
        if (t > 0) { hitOut = origin + dir * t; return true; }
    }
    return false;
}

inline std::string lowerExt(const std::string& path) {
    auto ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}

inline bool projectToScreen(const glm::mat4& vp, const ImVec2& viewportMin,
                            float vpW, float vpH,
                            const glm::vec3& world, ImVec2& out) {
    glm::vec4 clip = vp * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out.x = viewportMin.x + (ndc.x * 0.5f + 0.5f) * vpW;
    out.y = viewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpH;
    return true;
}

inline float distToSegment2D(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    float ax = a.x, ay = a.y;
    float bx = b.x, by = b.y;
    float px = p.x, py = p.y;

    float vx = bx - ax, vy = by - ay;
    float wx = px - ax, wy = py - ay;

    float vv = vx*vx + vy*vy;
    float t = (vv > 1e-6f) ? ((wx*vx + wy*vy) / vv) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float cx = ax + vx * t;
    float cy = ay + vy * t;
    float dx = px - cx;
    float dy = py - cy;
    return std::sqrt(dx*dx + dy*dy);
}

} // namespace AppInternal