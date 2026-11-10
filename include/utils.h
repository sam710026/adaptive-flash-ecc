#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

bool read_plain_input(const std::string& path, std::size_t& n, std::vector<uint8_t>& data);
bool read_code_input(const std::string& path, std::size_t& n, std::vector<uint8_t>& code);
bool write_hex_output(const std::string& path, const std::vector<uint8_t>& data);

