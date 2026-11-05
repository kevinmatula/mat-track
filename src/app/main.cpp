#include <cstdio>
#include <cstring>
#include <utility>

#include "app/app.hpp"

namespace {

void print_usage() {
  std::printf(
      "usage: mat-track [--data-dir <path>]\n"
      "\n"
      "  --data-dir <path>  store the database and config here instead of\n"
      "                     ~/.local/share/mat-track/\n"
      "  -h, --help         show this help\n");
}

}  // namespace

int main(int argc, char** argv) {
  mattrack::AppOptions options;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--data-dir") == 0) {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "mat-track: --data-dir needs a path\n");
        return 2;
      }
      options.data_dir = argv[++i];
    } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      print_usage();
      return 0;
    } else {
      std::fprintf(stderr, "mat-track: unknown argument '%s'\n", argv[i]);
      print_usage();
      return 2;
    }
  }
  return mattrack::App(std::move(options)).run();
}
