#pragma once

#include <cstddef>

// Files from assets/, compiled in by cmake/EmbedFile.cmake.
namespace mattrack::assets {
extern const unsigned char font_ttf[];
extern const std::size_t font_ttf_size;
extern const unsigned char chime_wav[];
extern const std::size_t chime_wav_size;
extern const unsigned char break_end_wav[];
extern const std::size_t break_end_wav_size;
}  // namespace mattrack::assets
