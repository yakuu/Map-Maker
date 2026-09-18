#include "AssetRegistry.h"
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

bool AssetRegistry::isSupported(const std::string& ext) {
    static const char* kExts[] = {
        ".obj", ".fbx", ".gltf", ".glb", ".dae", ".ply", ".stl", ".3ds"
    };
    for (auto* e : kExts) if (ext == e) return true;
    return false;
}

size_t AssetRegistry::hashPath(const std::string& fullPath) {
    return std::hash<std::string>{}(fullPath);
}

int AssetRegistry::scan(const std::string& root) {
    assets.clear();
    rootPath = root;

    std::error_code ec;
    if (!fs::exists(root, ec)) return 0;

    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file()) continue;

        auto ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (!isSupported(ext)) continue;

        AssetEntry e;
        e.fullPath = it->path().string();
        e.name     = it->path().filename().string();
        e.path     = fs::relative(it->path(), root, ec).string();
        if (e.path.empty()) e.path = e.name;
        std::replace(e.path.begin(), e.path.end(), '\\', '/');
        e.hash     = hashPath(e.fullPath);
        assets.push_back(std::move(e));
    }

    std::sort(assets.begin(), assets.end(),
              [](const AssetEntry& a, const AssetEntry& b) {
                  return a.path < b.path;
              });
    return (int)assets.size();
}

AssetEntry* AssetRegistry::find(size_t hash) {
    for (auto& a : assets) if (a.hash == hash) return &a;
    return nullptr;
}

AssetEntry* AssetRegistry::findByPath(const std::string& fullPath) {
    for (auto& a : assets) if (a.fullPath == fullPath) return &a;
    return nullptr;
}