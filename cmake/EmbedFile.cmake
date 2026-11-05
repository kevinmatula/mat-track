# Turns a binary file into a C++ source defining
#   mattrack::assets::<NAME>[] and mattrack::assets::<NAME>_size.
# Usage: cmake -DIN=<file> -DOUT=<file.cpp> -DNAME=<identifier> -P EmbedFile.cmake
file(READ "${IN}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
file(WRITE "${OUT}"
  "// Generated from ${IN}. Do not edit.\n"
  "#include <cstddef>\n"
  "namespace mattrack::assets {\n"
  "extern const unsigned char ${NAME}[] = {${bytes}};\n"
  "extern const std::size_t ${NAME}_size = sizeof(${NAME});\n"
  "}  // namespace mattrack::assets\n")
