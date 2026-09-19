#include "Scene.h"
#include <algorithm>

void Scene::initGat(int w, int h) {
    gatW = w; gatH = h;
    gat.assign((size_t)w * h, GatCell::Walkable);
    textureLayers.assign((size_t)w * h, 0);
}

void Scene::initHeightmap(int w, int h) {
    hmW = w; hmH = h;
    heightmap.assign((size_t)w * h, 0.0f);
    heightmapVersion++;
}

Instance* Scene::find(int id) {
    auto it = idIndex.find(id);
    if (it == idIndex.end()) return nullptr;

    size_t idx = it->second;
    // Defensive: if someone mutated `instances` directly, the index may be
    // stale. Rebuild once and retry rather than returning garbage.
    if (idx >= instances.size() || instances[idx].id != id) {
        rebuildIdIndex();
        it = idIndex.find(id);
        if (it == idIndex.end()) return nullptr;
        idx = it->second;
    }
    return &instances[idx];
}

int Scene::addInstance(Instance inst) {
    inst.id = nextId++;
    instances.push_back(std::move(inst));
    idIndex[instances.back().id] = instances.size() - 1;
    return instances.back().id;
}

bool Scene::removeInstance(int id) {
    auto it = idIndex.find(id);
    if (it == idIndex.end()) return false;

    size_t idx = it->second;
    if (idx >= instances.size() || instances[idx].id != id) return false;

    // Preserve insertion order so the scene list panel stays stable.
    // This is O(n) in the worst case, but removeInstance is rare; find() is
    // the hot path and is now O(1).
    instances.erase(instances.begin() + (std::ptrdiff_t)idx);
    idIndex.erase(it);
    for (size_t i = idx; i < instances.size(); ++i) {
        idIndex[instances[i].id] = i;
    }
    return true;
}

void Scene::rebuildIdIndex() {
    idIndex.clear();
    idIndex.reserve(instances.size());
    for (size_t i = 0; i < instances.size(); ++i) {
        idIndex[instances[i].id] = i;
    }
}

void Scene::clear() {
    instances.clear();
    idIndex.clear();
    nextId = 1;
}

void Scene::resizeGat(int newW, int newH, bool preserve) {
    if (newW < 1 || newH < 1) return;
    if (newW == gatW && newH == gatH) return;

    std::vector<GatCell> newGat((size_t)newW * newH, GatCell::Walkable);
    std::vector<uint8_t> newLayers((size_t)newW * newH, 0);

    if (preserve && !gat.empty()) {
        const int copyW = std::min(gatW, newW);
        const int copyH = std::min(gatH, newH);
        for (int y = 0; y < copyH; ++y) {
            for (int x = 0; x < copyW; ++x) {
                newGat[(size_t)y * newW + x]    = gat[(size_t)y * gatW + x];
                newLayers[(size_t)y * newW + x] = textureLayers[(size_t)y * gatW + x];
            }
        }
    }

    gatW = newW; gatH = newH;
    gat = std::move(newGat);
    textureLayers = std::move(newLayers);
}

void Scene::resizeHeightmap(int newW, int newH, bool preserve) {
    if (newW < 2 || newH < 2) return;
    if (newW == hmW && newH == hmH) return;

    std::vector<float> newHm((size_t)newW * newH, 0.0f);

    if (preserve && !heightmap.empty()) {
        const int copyW = std::min(hmW, newW);
        const int copyH = std::min(hmH, newH);
        for (int y = 0; y < copyH; ++y) {
            for (int x = 0; x < copyW; ++x) {
                newHm[(size_t)y * newW + x] = heightmap[(size_t)y * hmW + x];
            }
        }
    }

    hmW = newW; hmH = newH;
    heightmap = std::move(newHm);
    heightmapVersion++;
}