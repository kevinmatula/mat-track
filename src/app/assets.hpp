#pragma once

#include <cstddef>

// Files from assets/, compiled in by cmake/EmbedFile.cmake.
namespace mattrack::assets {
extern const unsigned char font_ttf[];
extern const std::size_t font_ttf_size;
}  // namespace mattrack::assets
