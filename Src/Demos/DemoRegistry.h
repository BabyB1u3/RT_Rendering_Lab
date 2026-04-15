#pragma once

/// @file DemoRegistry.h
/// @brief Global registry mapping demo names to factory functions.
///
/// Demos register themselves via Register(name, factory) during LabLayer::OnAttach().
/// Create(name) invokes the matching factory to produce a fresh DemoBase instance.
/// GetNames() returns insertion-ordered names for UI display (DemoSelectorPanel).
///
/// Storage uses function-local statics (Entries(), Names()) to avoid the
/// static initialization order fiasco (SIOF).

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Core/Util/Base.h"

class DemoBase;

class DemoRegistry
{
public:
    using Factory = std::function<Scope<DemoBase>()>;

    static void Register(const std::string& name, Factory factory);
    static Scope<DemoBase> Create(const std::string& name);
    static const std::vector<std::string>& GetNames();

private:
    struct Entry
    {
        std::string Name;
        Factory Create;
    };

    static std::vector<Entry>& Entries();
    static std::vector<std::string>& Names();
};
