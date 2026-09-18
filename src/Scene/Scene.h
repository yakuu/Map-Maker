#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

enum class GatCell : uint8_t {
    Walkable = 0,
    NotWalkable = 1,
    EventWalkable = 2
};

struct Instance {
    int id = 0;
    size_t meshHash = 0;
    std::string meshName;
    glm::vec3 position{ 0.0f };
    glm::vec3 rotation{ 0.0f };
    glm::vec3 scale{ 1.0f };
    glm::vec4 tint{ 1.0f };
    int materialIndex = 0;
};

class Scene {
public:
    std::vector<Instance> instances;

    // GAT grid
    int gatW = 64, gatH = 64;
    float cellSize = 1.0f;
    glm::vec3 gatOrigin{ -32.0f, 0.0f, -32.0f };
    std::vector<GatCell> gat;

    // Texture layer grid (same dimensions as GAT)
    std::vector<uint8_t> textureLayers;

    // Heightmap
    int hmW = 33, hmH = 33;
    float hmCell = 1.5f;
    glm::vec3 hmOrigin{ -24.0f, 0.0f, -24.0f };
    std::vector<float> heightmap;
    int heightmapVersion = 0;

    void initGat(int w, int h);
    void initHeightmap(int w, int h);

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < gatW && y < gatH;
    }
    GatCell& at(int x, int y) { return gat[y * gatW + x]; }
    GatCell  at(int x, int y) const { return gat[y * gatW + x]; }

    glm::vec3 cellCenter(int x, int y) const {
        return gatOrigin + glm::vec3((x + 0.5f) * cellSize, 0.0f,
                                     (y + 0.5f) * cellSize);
    }

    bool  hmInBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < hmW && y < hmH;
    }
    float& hmAt(int x, int y) { return heightmap[y * hmW + x]; }
    float  hmAt(int x, int y) const { return heightmap[y * hmW + x]; }
    glm::vec3 hmWorldPos(int x, int y) const {
        return hmOrigin + glm::vec3(x * hmCell, hmAt(x, y), y * hmCell);
    }

    Instance* find(int id);
    int addInstance(Instance inst);
    bool removeInstance(int id);

    void clear();

private:
    int nextId = 1;
};