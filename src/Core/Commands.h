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

class InstanceStateCommand : public Command {
public:
    InstanceStateCommand(Scene* scene, Instance before, Instance after,
                         const char* commandName = "Edit Instance")
        : scene(scene), before(std::move(before)), after(std::move(after)),
          commandName(commandName) {}
    void apply() override {
        if (auto* instance = scene->find(after.id)) *instance = after;
    }
    void revert() override {
        if (auto* instance = scene->find(before.id)) *instance = before;
    }
    const char* name() const override { return commandName; }
private:
    Scene* scene;
    Instance before;
    Instance after;
    const char* commandName;
};

class InstancePresenceCommand : public Command {
public:
    InstancePresenceCommand(Scene* scene, Instance instance, size_t index,
                            bool presentAfter, const char* commandName)
        : scene(scene), instance(std::move(instance)), index(index),
          presentAfter(presentAfter), commandName(commandName) {}
    void apply() override { setPresent(presentAfter); }
    void revert() override { setPresent(!presentAfter); }
    const char* name() const override { return commandName; }
private:
    void setPresent(bool present) {
        if (present) {
            if (!scene->find(instance.id)) scene->insertInstance(instance, index);
        } else {
            scene->removeInstance(instance.id);
        }
    }

    Scene* scene;
    Instance instance;
    size_t index;
    bool presentAfter;
    const char* commandName;
};

class InstanceBatchPresenceCommand : public Command {
public:
    InstanceBatchPresenceCommand(Scene* scene, std::vector<Instance> instances,
                                 size_t index, bool presentAfter)
        : scene(scene), instances(std::move(instances)), index(index),
          presentAfter(presentAfter) {}
    void apply() override { setPresent(presentAfter); }
    void revert() override { setPresent(!presentAfter); }
    const char* name() const override { return "Reseed Scene"; }
private:
    void setPresent(bool present) {
        if (present) {
            for (size_t i = 0; i < instances.size(); ++i)
                if (!scene->find(instances[i].id))
                    scene->insertInstance(instances[i], index + i);
        } else {
            for (auto it = instances.rbegin(); it != instances.rend(); ++it)
                scene->removeInstance(it->id);
        }
    }

    Scene* scene;
    std::vector<Instance> instances;
    size_t index;
    bool presentAfter;
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

class HeightmapStrokeCommand : public Command {
public:
    struct VertexEdit { int x, y; float before, after; };
    HeightmapStrokeCommand(Scene* scene, std::vector<VertexEdit> edits)
        : scene(scene), edits(std::move(edits)) {}
    void apply() override { setValues(true); }
    void revert() override { setValues(false); }
    const char* name() const override { return "Landscape Stroke"; }
private:
    void setValues(bool useAfter) {
        for (const auto& edit : edits) {
            if (scene->hmInBounds(edit.x, edit.y))
                scene->hmAt(edit.x, edit.y) = useAfter ? edit.after : edit.before;
        }
        ++scene->heightmapVersion;
    }

    Scene* scene;
    std::vector<VertexEdit> edits;
};

class TextureStrokeCommand : public Command {
public:
    struct CellEdit { int x, y; uint8_t before, after; };
    TextureStrokeCommand(Scene* scene, std::vector<CellEdit> edits)
        : scene(scene), edits(std::move(edits)) {}
    void apply() override { setValues(true); }
    void revert() override { setValues(false); }
    const char* name() const override { return "Texture Stroke"; }
private:
    void setValues(bool useAfter) {
        for (const auto& edit : edits) {
            if (scene->inBounds(edit.x, edit.y)) {
                scene->textureLayers[(size_t)edit.y * scene->gatW + edit.x] =
                    useAfter ? edit.after : edit.before;
            }
        }
    }

    Scene* scene;
    std::vector<CellEdit> edits;
};

class HeightmapResizeCommand : public Command {
public:
    struct State {
        int width;
        int height;
        float cellSize;
        glm::vec3 origin;
        std::vector<float> heights;
    };

    HeightmapResizeCommand(Scene* scene, State before, State after)
        : scene(scene), before(std::move(before)), after(std::move(after)) {}
    void apply() override { setState(after); }
    void revert() override { setState(before); }
    const char* name() const override { return "Resize Heightmap"; }
private:
    void setState(const State& state) {
        scene->hmW = state.width;
        scene->hmH = state.height;
        scene->hmCell = state.cellSize;
        scene->hmOrigin = state.origin;
        scene->heightmap = state.heights;
        ++scene->heightmapVersion;
    }

    Scene* scene;
    State before;
    State after;
};