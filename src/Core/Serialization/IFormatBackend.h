#pragma once

/// @file IFormatBackend.h
/// @brief Interface for format backends that convert PropertyTree to/from wire format.

#include "core/serialization/PropertyTree.h"
#include <string>

namespace Serialization
{

    class IFormatBackend
    {
    public:
        virtual ~IFormatBackend() = default;

        /// Serialize a PropertyTree to a string representation.
        virtual std::string WriteToString(const PropertyTree &tree) const = 0;

        /// Deserialize from a string into a PropertyTree.
        /// Returns false on parse error; details logged via LOG_ERROR.
        virtual bool ReadFromString(const std::string &data, PropertyTree &tree) const = 0;
    };

} // namespace Serialization
