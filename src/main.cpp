#include "ecc.h"
#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int parse_t(const char* text) {
    char* end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 0) {
        return -1;
    }
    return static_cast<int>(value);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        return 1;
    }

    const std::string mode = argv[1];
    const int t = parse_t(argv[2]);
    if (t < 0) {
        return 1;
    }

    const std::string input_path = argv[3];
    const std::string output_path = argv[4];

    std::size_t n = 0;
    std::vector<uint8_t> bytes;

    if (mode == "encode") {
        if (!read_plain_input(input_path, n, bytes)) {
            return 1;
        }
        std::vector<uint8_t> encoded = ecc_encode(bytes, t);
        return write_hex_output(output_path, encoded) ? 0 : 1;
    }

    if (mode == "decode") {
        if (!read_code_input(input_path, n, bytes)) {
            return 1;
        }
        std::vector<uint8_t> decoded = ecc_decode(bytes, n, t);
        return write_hex_output(output_path, decoded) ? 0 : 1;
    }

    return 1;
}

