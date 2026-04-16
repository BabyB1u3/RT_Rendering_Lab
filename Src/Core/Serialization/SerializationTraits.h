#pragma once

/// @file SerializationTraits.h
/// @brief Defines the Serializable concept for the trait-based serialization system.
///
/// Each serializable type provides a free function pair found via ADL:
///   void Serialize(PropertyTree &tree, const T &value);
///   bool Deserialize(const PropertyTree &tree, T &value);

#include "Core/Serialization/PropertyTree.h"
#include <concepts>

namespace Serialization
{

/// Concept: T is Serializable if free functions Serialize/Deserialize exist.
template <typename T>
concept Serializable = requires(PropertyTree& tree, const PropertyTree& ctree, T& val, const T& cval) {
    { Serialize(tree, cval) };
    { Deserialize(ctree, val) } -> std::same_as<bool>;
};

} // namespace Serialization
