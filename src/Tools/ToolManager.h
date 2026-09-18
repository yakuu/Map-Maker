#pragma once
#include "ITool.h"
#include <memory>
#include <vector>
#include <string>

class Application;

class ToolManager {
public:
    void init(Application& app);
    void shutdown();

    void update(Application& app, float dt);
    void onImGui(Application& app);
    void drawRadialMenu(Application& app);
    void drawStatusBar(Application& app);

    void setActive(int index);
    int  activeIndex() const { return activeIdx; }
    ITool* active() const;

    void addTool(std::unique_ptr<ITool> tool) {
        tools.push_back(std::move(tool));
    }

    const std::vector<std::unique_ptr<ITool>>& all() const { return tools; }

    bool radialOpen = false;
    int  radialHover = -1;

private:
    std::vector<std::unique_ptr<ITool>> tools;
    int activeIdx = 0;
};