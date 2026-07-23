// WindowRelation.cpp
#include "WindowRelation.h"
#include "WindowRuntime.h" // Include full definition for implementation

bool WindowRelation::IsRoot() const {
    return parent == nullptr;
}

bool WindowRelation::HasParent() const {
    return parent != nullptr;
}

bool WindowRelation::HasChildren() const {
    return !children.empty();
}

void WindowRelation::AddChild(WindowRuntime* child) {
    if (child) {
        // Optional: check for duplicates
        children.push_back(child);
    }
}

void WindowRelation::RemoveChild(WindowRuntime* child) {
    if (child) {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }
}