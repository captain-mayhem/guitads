# Writes a file's bytes as a C++ byte array, the same shape `xxd -i` gives
# the arrays in guires_data.cpp. Run in script mode by guit3/CMakeLists.txt:
#
#   cmake -DINPUT=<file> -DOUTPUT=<.cpp> -DNAME=<symbol prefix> -P embed_file.cmake
#
# defines <NAME>_data[] and <NAME>_size, declared in guires_data.h.

file(READ "${INPUT}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")
# twelve bytes to a line (CMake regexes have no {n} repetition)
string(REPEAT "0x[0-9a-f][0-9a-f], " 12 line)
string(REGEX REPLACE "(${line})" "\\1\n  " bytes "${bytes}")
string(REGEX REPLACE ", $" "" bytes "${bytes}")
string(REGEX REPLACE " +\n" "\n" bytes "${bytes}")

file(WRITE "${OUTPUT}"
"/*
 *   Generated at build time by guit3/embed_file.cmake from
 *   ${INPUT} - do not edit.
 */

#include \"guires_data.h\"

const unsigned char ${NAME}_data[] = {
  ${bytes}
};
const unsigned int ${NAME}_size = sizeof(${NAME}_data);
")
