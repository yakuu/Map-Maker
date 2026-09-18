#include "Config.h"
#include <fstream>
#include <cstdio>

Config::Json Config::emptyJson = Config::Json::object();

bool Config::loadDirectory(const std::string& dir) {
    root = dir;
    entries.clear();
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        std::filesystem::create_directories(root, ec);
    }
    for (auto& p : std::filesystem::directory_iterator(root, ec)) {
        if (!p.is_regular_file()) continue;
        if (p.path().extension() != ".json") continue;

        Entry e;
        e.path = p.path();
        e.mtime = std::filesystem::last_write_time(e.path, ec);
        std::ifstream in(e.path);
        try {
            in >> e.data;
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "[config] parse error %s: %s\n",
                         e.path.string().c_str(), ex.what());
            e.data = Json::object();
        }
        entries[e.path.stem().string()] = std::move(e);
    }
    return true;
}

void Config::poll() {
    std::error_code ec;
    for (auto& [name, e] : entries) {
        auto t = std::filesystem::last_write_time(e.path, ec);
        if (ec) continue;
        if (t != e.mtime) {
            e.mtime = t;
            std::ifstream in(e.path);
            try {
                in >> e.data;
                if (onReload) onReload(name);
            } catch (const std::exception& ex) {
                std::fprintf(stderr, "[config] reload error %s: %s\n",
                             e.path.string().c_str(), ex.what());
            }
        }
    }
}

Config::Json& Config::get(const std::string& name) {
    auto it = entries.find(name);
    if (it == entries.end()) return emptyJson;
    return it->second.data;
}

const Config::Json& Config::get(const std::string& name) const {
    auto it = entries.find(name);
    if (it == entries.end()) return emptyJson;
    return it->second.data;
}