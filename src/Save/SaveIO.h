#pragma once
#include <string>

class Scene;

namespace SaveIO {
    bool save(const std::string& path, const Scene& scene);
    bool load(const std::string& path, Scene& scene, std::string& errorOut);
}