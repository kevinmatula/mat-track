#include <cstdio>
#include <cstring>
#include <utility>

#include "app/app.hpp"

namespace {

void print_usage() {
  std::printf(
      "usage: mat-track [--data-dir <path>] [--seed-demo]\n"
      "\n"
      "  --data-dir <path>  store the database and config here instead of\n"
      "                     ~/.local/share/mat-track/\n"
      "  --seed-demo        fill an empty database with ~6 months of sample\n"
      "                     sessions (requires --data-dir)\n"
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
    } else if (std::strcmp(argv[i], "--seed-demo") == 0) {
      options.seed_demo = true;
    } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      print_usage();
      return 0;
    } else {
      std::fprintf(stderr, "mat-track: unknown argument '%s'\n", argv[i]);
      print_usage();
      return 2;
    }
  }
  if (options.seed_demo && !options.data_dir) {
    // Never mix sample data into the real history.
    std::fprintf(stderr, "mat-track: --seed-demo requires --data-dir\n");
    return 2;
  }
  return mattrack::App(std::move(options)).run();
}
