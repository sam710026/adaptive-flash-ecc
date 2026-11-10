#include "utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::string trim_copy(const std::string& s) {
    std::size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
        ++first;
    }

    std::size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }

    return s.substr(first, last - first);
}

bool parse_hex(const std::string& text, std::vector<uint8_t>& out) {
    std::string hex = trim_copy(text);
    if (hex.size() % 2 != 0) {
        return false;
    }

    out.clear();
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int hi = hex_value(hex[i]);
        int lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

bool read_input_common(const std::string& path, std::size_t& n, std::vector<uint8_t>& bytes) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::string line1;
    std::string line2;
    if (!std::getline(in, line1) || !std::getline(in, line2)) {
        return false;
    }

    std::istringstream iss(trim_copy(line1));
    if (!(iss >> n)) {
        return false;
    }

    return parse_hex(line2, bytes);
}

}  // namespace

bool read_plain_input(const std::string& path, std::size_t& n, std::vector<uint8_t>& data) {
    if (!read_input_common(path, n, data)) {
        return false;
    }
    return data.size() == n;
}

bool read_code_input(const std::string& path, std::size_t& n, std::vector<uint8_t>& code) {
    return read_input_common(path, n, code);
}

bool write_hex_output(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }

    static const char* digits = "0123456789ABCDEF";
    for (uint8_t byte : data) {
        out << digits[byte >> 4] << digits[byte & 0x0F];
    }
    out << '\n';
    return static_cast<bool>(out);
}

