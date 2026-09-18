#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

struct GLFWwindow;

class Shortcuts {
public:
    static constexpr int kMouseBase = 1000;

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    void update(GLFWwindow* window);

    bool isDown(const std::string& action) const;
    bool justPressed(const std::string& action) const;
    bool justReleased(const std::string& action) const;

    int  key(const std::string& action) const;
    void set(const std::string& action, int key);

    const std::unordered_map<std::string, int>& map() const { return bindings; }

    // helpers for UI
    static std::string keyName(int code);

private:
    std::string configPath;
    std::unordered_map<std::string, int> bindings;
    std::unordered_set<std::string> downNow;
    std::unordered_set<std::string> downPrev;
};