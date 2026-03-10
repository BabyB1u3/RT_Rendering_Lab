#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/Base.h"

class DemoBase;

class DemoRegistry
{
public:
    using Factory = std::function<Scope<DemoBase>()>;

    static void Register(const std::string &name, Factory factory);
    static Scope<DemoBase> Create(const std::string &name);
    static const std::vector<std::string> &GetNames();

private:
    struct Entry
    {
        std::string Name;
        Factory Create;
    };

    static std::vector<Entry> &Entries();
    static std::vector<std::string> &Names();
};