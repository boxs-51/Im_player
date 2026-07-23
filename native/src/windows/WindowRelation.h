// WindowRelation.h
#pragma once
#include <vector>
#include <memory>
#include <algorithm>

// Forward declarations
class WindowRuntime;
class WindowSharedGroup;

class WindowRelation {
public:
    WindowRuntime* parent = nullptr;
    std::vector<WindowRuntime*> children;
    std::weak_ptr<WindowSharedGroup> sharedGroup;

    void AddChild(WindowRuntime* child);
    void RemoveChild(WindowRuntime* child);

    bool IsRoot() const;
    bool HasParent() const;
    bool HasChildren() const;
};