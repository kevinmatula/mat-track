#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mattrack {

// Writes via a temp file + rename so a crash never leaves a half-written
// file. Throws std::runtime_error on failure.
void write_file_atomic(const std::filesystem::path& path, std::string_view contents);

// Whole file, or nullopt if it doesn't exist. Throws std::runtime_error if
// it exists but cannot be read.
std::optional<std::string> read_file(const std::filesystem::path& path);

}  // namespace mattrack
