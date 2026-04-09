#pragma once

/// @file JsonBackend.h
/// @brief JSON format backend using nlohmann/json.
///
/// This is the sole point of coupling to nlohmann/json in the engine.
/// No other file should include <json.hpp> for serialization purposes.

#include "Core/Serialization/IFormatBackend.h"

namespace Serialization
{

    class JsonBackend : public IFormatBackend
    {
    public:
        explicit JsonBackend(int indent = 2) : m_Indent(indent) {}

        std::string WriteToString(const PropertyTree &tree) const override;
        bool ReadFromString(const std::string &data, PropertyTree &tree) const override;

    private:
        int m_Indent;
    };

} // namespace Serialization
