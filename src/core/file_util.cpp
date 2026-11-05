#include "core/file_util.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mattrack {

void write_file_atomic(const std::filesystem::path& path, std::string_view contents) {
  const auto tmp = std::filesystem::path(path).concat(".tmp");
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + tmp.string());
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    out.flush();
    if (!out) throw std::runtime_error("cannot write " + tmp.string());
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    throw std::runtime_error("cannot replace " + path.string());
  }
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) return std::nullopt;
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + path.string());
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace mattrack
