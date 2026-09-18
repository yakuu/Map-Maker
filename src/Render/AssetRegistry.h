#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct AssetEntry {
    std::string path;       // relative to the scanned root, e.g. "models/brick.obj"
    std::string fullPath;   // OS path used to load
    std::string name;       // filename
    size_t      hash = 0;
    bool        loaded = false;
    int         vertexCount = 0;
    int         indexCount = 0;
};

class AssetRegistry {
public:
    // Recursively scans `root`. Replaces all previous entries.
    // Returns the number of supported files found.
    int scan(const std::string& root);

    const std::vector<AssetEntry>& entries() const { return assets; }
    std::vector<AssetEntry>& entries() { return assets; }

    AssetEntry* find(size_t hash);
    AssetEntry* findByPath(const std::string& fullPath);

    static bool isSupported(const std::string& ext);
    static size_t hashPath(const std::string& fullPath);

    const std::string& root() const { return rootPath; }

private:
    std::vector<AssetEntry> assets;
    std::string rootPath;
};