#include "SaveIO.h"
#include "Scene/Scene.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>

using nlohmann::json;

namespace SaveIO {

bool save(const std::string& path, const Scene& s) {
    json j;
    j["version"] = 2;

    json arr = json::array();
    for (auto& i : s.instances) {
        arr.push_back({
            {"id", i.id},
            {"meshHash", (uint64_t)i.meshHash},
            {"meshName", i.meshName},
            {"position", {i.position.x, i.position.y, i.position.z}},
            {"rotation", {i.rotation.x, i.rotation.y, i.rotation.z}},
            {"scale",    {i.scale.x, i.scale.y, i.scale.z}},
            {"tint",     {i.tint.x, i.tint.y, i.tint.z, i.tint.w}},
            {"materialIndex", i.materialIndex},
        });
    }
    j["instances"] = std::move(arr);

    json gatObj;
    gatObj["w"] = s.gatW;
    gatObj["h"] = s.gatH;
    gatObj["cellSize"] = s.cellSize;
    gatObj["origin"] = { s.gatOrigin.x, s.gatOrigin.y, s.gatOrigin.z };

    json cells = json::array();
    for (size_t k = 0; k < s.gat.size(); ++k) cells.push_back((int)s.gat[k]);
    gatObj["cells"] = std::move(cells);

    json layers = json::array();
    for (size_t k = 0; k < s.textureLayers.size(); ++k)
        layers.push_back((int)s.textureLayers[k]);
    gatObj["textureLayers"] = std::move(layers);

    j["gat"] = std::move(gatObj);

    json hm;
    hm["w"] = s.hmW;
    hm["h"] = s.hmH;
    hm["cell"] = s.hmCell;
    hm["origin"] = { s.hmOrigin.x, s.hmOrigin.y, s.hmOrigin.z };
    json heights = json::array();
    for (float v : s.heightmap) heights.push_back(v);
    hm["heights"] = std::move(heights);
    j["heightmap"] = std::move(hm);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << j.dump(2);
    return true;
}

bool load(const std::string& path, Scene& s, std::string& errorOut) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { errorOut = "Cannot open " + path; return false; }

    json j;
    try { in >> j; }
    catch (const std::exception& e) { errorOut = e.what(); return false; }

    try {
        s.clear();
        s.instances.clear();

        if (j.contains("instances")) {
            for (auto& e : j["instances"]) {
                Instance i;
                i.id       = e.value("id", 0);
                i.meshHash = (size_t)e.value("meshHash", (uint64_t)0);
                i.meshName = e.value("meshName", std::string{});
                i.materialIndex = e.value("materialIndex", 0);

                auto p  = e.value("position", std::vector<float>{0,0,0});
                auto r  = e.value("rotation", std::vector<float>{0,0,0});
                auto sc = e.value("scale",    std::vector<float>{1,1,1});
                auto t  = e.value("tint",     std::vector<float>{1,1,1,1});

                if (p.size()  == 3) i.position = { p[0], p[1], p[2] };
                if (r.size()  == 3) i.rotation = { r[0], r[1], r[2] };
                if (sc.size() == 3) i.scale    = { sc[0], sc[1], sc[2] };
                if (t.size()  == 4) i.tint     = { t[0], t[1], t[2], t[3] };
                s.instances.push_back(i);
            }
        }

        if (j.contains("gat")) {
            auto& g = j["gat"];
            s.gatW = g.value("w", 64);
            s.gatH = g.value("h", 64);
            s.cellSize = g.value("cellSize", 1.0f);

            auto o = g.value("origin", std::vector<float>{-32,0,-32});
            if (o.size() == 3) s.gatOrigin = { o[0], o[1], o[2] };

            s.gat.assign((size_t)s.gatW * s.gatH, GatCell::Walkable);
            s.textureLayers.assign((size_t)s.gatW * s.gatH, 0);

            if (g.contains("cells")) {
                size_t k = 0;
                for (auto& c : g["cells"]) {
                    if (k >= s.gat.size()) break;
                    s.gat[k++] = (GatCell)c.get<int>();
                }
            }
            if (g.contains("textureLayers")) {
                size_t k = 0;
                for (auto& c : g["textureLayers"]) {
                    if (k >= s.textureLayers.size()) break;
                    s.textureLayers[k++] = (uint8_t)c.get<int>();
                }
            }
        } else {
            s.initGat(64, 64);
        }

        if (j.contains("heightmap")) {
            auto& h = j["heightmap"];
            s.hmW = h.value("w", 33);
            s.hmH = h.value("h", 33);
            s.hmCell = h.value("cell", 1.5f);
            auto o = h.value("origin", std::vector<float>{-24,0,-24});
            if (o.size() == 3) s.hmOrigin = { o[0], o[1], o[2] };

            s.heightmap.assign((size_t)s.hmW * s.hmH, 0.0f);
            if (h.contains("heights")) {
                size_t k = 0;
                for (auto& v : h["heights"]) {
                    if (k >= s.heightmap.size()) break;
                    s.heightmap[k++] = v.get<float>();
                }
            }
            s.heightmapVersion++;
        } else {
            s.initHeightmap(33, 33);
        }
    } catch (const std::exception& e) {
        errorOut = e.what();
        return false;
    }
    return true;
}

}