#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

std::vector<uint8_t> ecc_encode(const std::vector<uint8_t>& message, int t);
std::vector<uint8_t> ecc_decode(const std::vector<uint8_t>& codeword, std::size_t message_size, int t);

