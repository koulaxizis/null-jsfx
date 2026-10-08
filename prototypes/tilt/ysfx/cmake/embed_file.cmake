# cmake -DINPUT=<file> -DOUTPUT=<file.cpp> -P embed_file.cmake
# Writes the bytes of INPUT as a C++ array (null_embedded_jsfx / _size).
file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hexlen)
math(EXPR size "${hexlen} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){32})" "\\1\n" bytes "${bytes}")
file(WRITE "${OUTPUT}.tmp"
"// Generated from ${INPUT} - do not edit.\n#include <cstddef>\n"
"extern const unsigned char null_embedded_jsfx[] = {\n${bytes}0x00};\n"
"extern const std::size_t null_embedded_jsfx_size = ${size};\n")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
