#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Resource
{
std::optional<std::string> ReadTextFile(const std::filesystem::path& path);
std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::filesystem::path& path);
bool WriteTextFile(const std::filesystem::path& path, std::string_view data);
bool WriteBinaryFile(const std::filesystem::path& path, std::span<const uint8_t> data);
} // namespace Resource
