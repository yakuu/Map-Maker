#pragma once
#include <memory>
#include <vector>
#include <string>

class Command {
public:
    virtual ~Command() = default;
    virtual void apply() = 0;
    virtual void revert() = 0;
    virtual const char* name() const = 0;
};

class CommandStack {
public:
    void push(std::unique_ptr<Command> cmd) {
        cmd->apply();
        redoStack.clear();
        undoStack.push_back(std::move(cmd));
    }

    bool undo() {
        if (undoStack.empty()) return false;
        auto cmd = std::move(undoStack.back());
        undoStack.pop_back();
        cmd->revert();
        redoStack.push_back(std::move(cmd));
        return true;
    }

    bool redo() {
        if (redoStack.empty()) return false;
        auto cmd = std::move(redoStack.back());
        redoStack.pop_back();
        cmd->apply();
        undoStack.push_back(std::move(cmd));
        return true;
    }

    void clear() { undoStack.clear(); redoStack.clear(); }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    size_t undoSize() const { return undoStack.size(); }
    size_t redoSize() const { return redoStack.size(); }

private:
    std::vector<std::unique_ptr<Command>> undoStack;
    std::vector<std::unique_ptr<Command>> redoStack;
};

/* ---- common concrete commands ---- */

#include "Scene/Scene.h"
#include <glm/glm.hpp>

class MoveInstanceCommand : public Command {
public:
    MoveInstanceCommand(Scene* s, int id, const glm::vec3& before, const glm::vec3& after)
        : scene(s), id(id), before(before), after(after) {}
    void apply() override  { if (auto* i = scene->find(id)) i->position = after; }
    void revert() override { if (auto* i = scene->find(id)) i->position = before; }
    const char* name() const override { return "Move Instance"; }
private:
    Scene* scene; int id;
    glm::vec3 before, after;
};

class GatStrokeCommand : public Command {
public:
    struct CellEdit { int x, y; GatCell before, after; };

    GatStrokeCommand(Scene* s, std::vector<CellEdit> edits)
        : scene(s), edits(std::move(edits)) {}

    void apply() override {
        for (auto& e : edits) if (scene->inBounds(e.x, e.y)) scene->at(e.x, e.y) = e.after;
    }
    void revert() override {
        for (auto& e : edits) if (scene->inBounds(e.x, e.y)) scene->at(e.x, e.y) = e.before;
    }
    const char* name() const override { return "GAT Stroke"; }
    bool empty() const { return edits.empty(); }
private:
    Scene* scene;
    std::vector<CellEdit> edits;
};