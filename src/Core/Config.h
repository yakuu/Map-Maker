#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

class Config {
public:
    using Json = nlohmann::json;

    bool loadDirectory(const std::string& dir);
    void poll();

    Json& get(const std::string& name);
    const Json& get(const std::string& name) const;

    std::function<void(const std::string& name)> onReload;

private:
    struct Entry {
        std::filesystem::path path;
        Json data;
        std::filesystem::file_time_type mtime{};
    };
    std::unordered_map<std::string, Entry> entries;
    std::filesystem::path root;
    static Json emptyJson;
};