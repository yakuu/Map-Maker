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
    for (auto& i : instances) if (i.id == id) return &i;
    return nullptr;
}

int Scene::addInstance(Instance inst) {
    inst.id = nextId++;
    instances.push_back(std::move(inst));
    return instances.back().id;
}

bool Scene::removeInstance(int id) {
    auto it = std::remove_if(instances.begin(), instances.end(),
                             [id](const Instance& i) { return i.id == id; });
    if (it == instances.end()) return false;
    instances.erase(it, instances.end());
    return true;
}

void Scene::clear() {
    instances.clear();
    nextId = 1;
}