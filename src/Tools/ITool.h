#pragma once
#include <string>

class Application;

class ITool {
public:
    virtual ~ITool() = default;

    virtual const char* name() const = 0;
    virtual const char* description() const { return ""; }
    virtual const char* statusHint() const { return ""; }

    virtual void onActivate(Application&) {}
    virtual void onDeactivate(Application&) {}
    virtual void onUpdate(Application&, float) {}

    virtual void onMouseDown(Application&, int /*button*/, float /*x*/, float /*y*/) {}
    virtual void onMouseMove(Application&, float /*x*/, float /*y*/) {}
    virtual void onMouseUp(Application&, int /*button*/, float /*x*/, float /*y*/) {}

    virtual void onImGui(Application&) {}
};