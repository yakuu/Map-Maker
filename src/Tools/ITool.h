#pragma once
#ifndef MAPMAKER_TOOLS_ITOOL_H
#define MAPMAKER_TOOLS_ITOOL_H

#include <string>

class Application;

class ITool {
public:
    virtual ~ITool() = default;

    virtual const char* name() const = 0;
    virtual const char* description() const { return ""; }
    virtual const char* statusHint() const { return ""; }

    virtual bool hasQuickMenu() const { return false; }
    virtual void drawQuickMenu(Application&) {}

    // Cell radius of the brush, for tools that paint into a grid. -1 means
    // "no brush cursor". 0 is a single 1x1 cell.
    virtual int brushCellRadius() const { return -1; }

    virtual void onActivate(Application&) {}
    virtual void onDeactivate(Application&) {}
    virtual void onUpdate(Application&, float) {}

    virtual void onMouseDown(Application&, int, float, float) {}
    virtual void onMouseMove(Application&, float, float) {}
    virtual void onMouseUp(Application&, int, float, float) {}

    virtual bool onMouseWheel(Application&, float) { return false; }

    virtual void onImGui(Application&) {}
};

#endif // MAPMAKER_TOOLS_ITOOL_H