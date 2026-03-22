#include "DemoRegistry.h"

#include <stdexcept>

#include "DemoBase.h"
#include "core/Logger.h"

// Function-local statics ensure the containers are initialized on first use,
// avoiding the static initialization order fiasco (SIOF).

std::vector<DemoRegistry::Entry> &DemoRegistry::Entries()
{
    static std::vector<Entry> s_Entries;
    return s_Entries;
}

std::vector<std::string> &DemoRegistry::Names()
{
    static std::vector<std::string> s_Names;
    return s_Names;
}

void DemoRegistry::Register(const std::string &name, Factory factory)
{
    auto &entries = Entries();

    // Silently ignore duplicate registrations (idempotent).
    for (const auto &entry : entries)
    {
        if (entry.Name == name)
            return;
    }

    entries.push_back({name, std::move(factory)});
    Names().push_back(name);
}

Scope<DemoBase> DemoRegistry::Create(const std::string &name)
{
    for (const auto &entry : Entries())
    {
        if (entry.Name == name)
            return entry.Create();
    }

    LOG_ERROR("DemoRegistry: unknown demo \"{}\"", name);
    throw std::runtime_error("DemoRegistry::Create failed. Unknown demo: " + name);
}

const std::vector<std::string> &DemoRegistry::GetNames()
{
    return Names();
}