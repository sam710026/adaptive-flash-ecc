#include "ecc.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <vector>

namespace {

constexpr uint16_t INF = 30000;

constexpr int SMALL_K = 11;
constexpr int SMALL_G0 = 02335;
constexpr int SMALL_G1 = 03661;
constexpr int LARGE_K = 12;
constexpr int LARGE_G0 = 04335;
constexpr int LARGE_G1 = 05723;

#ifndef ECC_LARGE_THRESHOLD_BYTES
#define ECC_LARGE_THRESHOLD_BYTES 256
#endif

#ifndef ECC_USE_BITREV_INTERLEAVER
#define ECC_USE_BITREV_INTERLEAVER 1
#endif

constexpr std::size_t LARGE_CODE_THRESHOLD_BYTES = ECC_LARGE_THRESHOLD_BYTES;

#ifndef ECC_SMALL_SALT
#define ECC_SMALL_SALT 1ULL
#endif

#ifndef ECC_MEDIUM_SALT
#define ECC_MEDIUM_SALT 127ULL
#endif

#ifndef ECC_LARGE_SALT
#define ECC_LARGE_SALT 7ULL
#endif

#ifndef ECC_LARGE_ALT_SALT
#define ECC_LARGE_ALT_SALT 29ULL
#endif

#ifndef ECC_SMALL_SIZE_LIMIT
#define ECC_SMALL_SIZE_LIMIT 128
#endif

#ifndef ECC_SMALL_LOW_T_LIMIT
#define ECC_SMALL_LOW_T_LIMIT 100
#endif

#ifndef ECC_SMALL_LOW_T_MILLI
#define ECC_SMALL_LOW_T_MILLI 1500
#endif

#ifndef ECC_SMALL_MID_T_LIMIT
#define ECC_SMALL_MID_T_LIMIT 200
#endif

#ifndef ECC_SMALL_MID_T_MILLI
#define ECC_SMALL_MID_T_MILLI 1750
#endif

#ifndef ECC_GENERAL_LOW_T_LIMIT
#define ECC_GENERAL_LOW_T_LIMIT 50
#endif

#ifndef ECC_GENERAL_LOW_T_MILLI
#define ECC_GENERAL_LOW_T_MILLI 1500
#endif

#ifndef ECC_GENERAL_MID_T_LIMIT
#define ECC_GENERAL_MID_T_LIMIT 100
#endif

#ifndef ECC_GENERAL_MID_T_MILLI
#define ECC_GENERAL_MID_T_MILLI 1750
#endif

uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

uint64_t interleaver_salt(std::size_t length, int t) {
    if (length <= 2048) {
        return static_cast<uint64_t>(ECC_SMALL_SALT);
    }
    if (length <= 4096) {
        return static_cast<uint64_t>(ECC_MEDIUM_SALT);
    }
    if ((std::max(t, 0) % 3) == 2) {
        return static_cast<uint64_t>(ECC_LARGE_ALT_SALT);
    }
    return static_cast<uint64_t>(ECC_LARGE_SALT);
}

int parity(int x) {
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x &= 0xF;
    return (0x6996 >> x) & 1;
}

uint8_t get_bit(const std::vector<uint8_t>& bytes, std::size_t bit_index) {
    if ((bit_index >> 3) >= bytes.size()) {
        return 0;
    }
    return static_cast<uint8_t>((bytes[bit_index >> 3] >> (7 - (bit_index & 7))) & 1U);
}

void set_bit(std::vector<uint8_t>& bytes, std::size_t bit_index, uint8_t bit) {
    if (bit != 0) {
        bytes[bit_index >> 3] = static_cast<uint8_t>(bytes[bit_index >> 3] | (1U << (7 - (bit_index & 7))));
    }
}

std::vector<uint8_t> bits_to_bytes(const std::vector<uint8_t>& bits, std::size_t byte_count) {
    std::vector<uint8_t> bytes(byte_count, 0);
    const std::size_t usable_bits = std::min(bits.size(), byte_count * 8);
    for (std::size_t i = 0; i < usable_bits; ++i) {
        bytes[i >> 3] = static_cast<uint8_t>(bytes[i >> 3] | (bits[i] << (7 - (i & 7))));
    }
    return bytes;
}

uint64_t choose_stride(std::size_t length, int t) {
    if (length <= 1) {
        return 1;
    }

    uint64_t seed = static_cast<uint64_t>(length) * 0xD1B54A32D192ED03ULL;
    seed ^= interleaver_salt(length, t);
    seed ^= static_cast<uint64_t>(std::max(t, 0)) * 0x94D049BB133111EBULL;
    uint64_t stride = (splitmix64(seed) % length) | 1ULL;
    while (std::gcd<uint64_t>(stride, length) != 1) {
        stride += 2;
        if (stride >= length) {
            stride = (stride % length) | 1ULL;
        }
    }
    return stride;
}

uint64_t choose_offset(std::size_t length, int t) {
    if (length == 0) {
        return 0;
    }
    uint64_t seed = static_cast<uint64_t>(length) * 0xA24BAED4963EE407ULL;
    uint64_t salt = interleaver_salt(length, t);
    seed ^= (salt << 1) | (salt >> 63);
    seed ^= static_cast<uint64_t>(std::max(t, 0) + 17) * 0x9FB21C651E98DF25ULL;
    return splitmix64(seed) % length;
}

bool is_power_of_two(std::size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

int log2_exact(std::size_t x) {
    int bits = 0;
    while ((std::size_t{1} << bits) < x) {
        ++bits;
    }
    return bits;
}

std::size_t bit_reverse_index(std::size_t x, int bits) {
    std::size_t y = 0;
    for (int i = 0; i < bits; ++i) {
        y = (y << 1) | (x & 1U);
        x >>= 1;
    }
    return y;
}

bool use_bitrev_interleaver(std::size_t length) {
    return ECC_USE_BITREV_INTERLEAVER && length >= 4096 && is_power_of_two(length);
}

struct InterleaverPlan {
    std::size_t length = 0;
    bool bitrev = false;
    int reverse_bits = 0;
    std::size_t bitrev_mask = 0;
    uint64_t stride = 1;
    uint64_t offset = 0;

    InterleaverPlan(std::size_t n, int t) : length(n) {
        if (length == 0) {
            return;
        }
        offset = choose_offset(length, t);
        bitrev = use_bitrev_interleaver(length);
        if (bitrev) {
            reverse_bits = log2_exact(length);
            bitrev_mask = length - 1;
            offset &= bitrev_mask;
        } else {
            stride = choose_stride(length, t);
        }
    }

    std::size_t physical_index(std::size_t logical_index) const {
        if (bitrev) {
            return bit_reverse_index((logical_index + offset) & bitrev_mask, reverse_bits);
        }
        return static_cast<std::size_t>((static_cast<uint64_t>(logical_index) * stride + offset) % length);
    }
};

std::size_t target_code_bytes(std::size_t message_size, int t) {
    if (message_size == 0) {
        return 0;
    }

    const int age = std::max(t, 0);
    int low_limit = ECC_GENERAL_LOW_T_LIMIT;
    int low_milli = ECC_GENERAL_LOW_T_MILLI;
    int mid_limit = ECC_GENERAL_MID_T_LIMIT;
    int mid_milli = ECC_GENERAL_MID_T_MILLI;

    if (message_size <= ECC_SMALL_SIZE_LIMIT) {
        low_limit = ECC_SMALL_LOW_T_LIMIT;
        low_milli = ECC_SMALL_LOW_T_MILLI;
        mid_limit = ECC_SMALL_MID_T_LIMIT;
        mid_milli = ECC_SMALL_MID_T_MILLI;
    }

    int milli = 2000;
    if (low_limit > 0 && age <= low_limit) {
        milli = low_milli;
    } else if (mid_limit > 0 && age <= mid_limit) {
        milli = mid_milli;
    }

    milli = std::clamp(milli, 1000, 2000);
    std::size_t bytes = (message_size * static_cast<std::size_t>(milli) + 999) / 1000;
    bytes = std::clamp(bytes, message_size, message_size * 2);
    return bytes;
}

template <int K, int G0, int G1>
std::vector<uint8_t> convolutional_encode_bytes(const std::vector<uint8_t>& message,
                                                std::size_t target_bits,
                                                int t) {
    constexpr int state_mask = (1 << (K - 1)) - 1;
    const std::size_t message_bits = message.size() * 8;
    const std::size_t tail_bits = K - 1;
    const std::size_t total_steps = message_bits + tail_bits;
    const std::size_t full_count = total_steps * 2;
    target_bits = std::min(target_bits, full_count);
    const std::size_t puncture_count = full_count - target_bits;
    std::vector<uint8_t> encoded_bytes((target_bits + 7) / 8, 0);
    InterleaverPlan interleaver(target_bits, t);

    int state = 0;
    std::size_t kept_index = 0;
    std::size_t puncture_index = 0;
    std::size_t next_puncture = puncture_count == 0
                                    ? full_count
                                    : ((puncture_index + 1) * full_count) / (puncture_count + 1);

    for (std::size_t step = 0; step < total_steps; ++step) {
        uint8_t bit = step < message_bits ? get_bit(message, step) : 0;
        int reg = ((state << 1) | (bit & 1)) & ((1 << K) - 1);
        uint8_t out0 = static_cast<uint8_t>(parity(reg & G0));
        uint8_t out1 = static_cast<uint8_t>(parity(reg & G1));
        for (int lane = 0; lane < 2; ++lane) {
            std::size_t full_pos = step * 2 + lane;
            if (full_pos == next_puncture) {
                ++puncture_index;
                next_puncture = puncture_index < puncture_count
                                    ? ((puncture_index + 1) * full_count) / (puncture_count + 1)
                                    : full_count;
            } else {
                std::size_t physical = interleaver.physical_index(kept_index);
                set_bit(encoded_bytes, physical, lane == 0 ? out0 : out1);
                ++kept_index;
            }
        }
        state = reg & state_mask;
    }

    return encoded_bytes;
}

template <int K, int G0, int G1>
struct Trellis {
    static constexpr int memory = K - 1;
    static constexpr int state_count = 1 << memory;
    std::array<uint16_t, state_count * 2> prev{};
    std::array<uint8_t, state_count * 2> out{};

    Trellis() {
        for (int state = 0; state < state_count; ++state) {
            int input_bit = state & 1;
            int prev0 = state >> 1;
            int prev1 = prev0 | (1 << (memory - 1));
            prev[state * 2] = static_cast<uint16_t>(prev0);
            prev[state * 2 + 1] = static_cast<uint16_t>(prev1);

            int reg0 = ((prev0 << 1) | input_bit) & ((1 << K) - 1);
            int reg1 = ((prev1 << 1) | input_bit) & ((1 << K) - 1);
            out[state * 2] = static_cast<uint8_t>((parity(reg0 & G0) << 1) | parity(reg0 & G1));
            out[state * 2 + 1] = static_cast<uint8_t>((parity(reg1 & G0) << 1) | parity(reg1 & G1));
        }
    }
};

constexpr uint8_t DIST[4][4] = {
    {0, 1, 1, 2},
    {1, 0, 2, 1},
    {1, 2, 0, 1},
    {2, 1, 1, 0},
};

template <int K, int G0, int G1>
std::vector<uint8_t> viterbi_decode_bytes(const std::vector<uint8_t>& codeword,
                                          std::size_t encoded_bits,
                                          std::size_t message_bits,
                                          int t) {
    constexpr int state_count = 1 << (K - 1);
    constexpr int top_prev_bit = 1 << (K - 2);
    static const Trellis<K, G0, G1> trellis;
    const std::size_t tail_bits = K - 1;
    const std::size_t total_steps = message_bits + tail_bits;
    const std::size_t full_count = total_steps * 2;
    const std::size_t observed_count = std::min(encoded_bits, full_count);
    const std::size_t puncture_count = full_count - observed_count;
    InterleaverPlan interleaver(observed_count, t);

    std::vector<uint16_t> metric(state_count, static_cast<uint16_t>(INF));
    std::vector<uint16_t> next_metric(state_count, 0);
    std::vector<uint8_t> previous_choice(total_steps * state_count, 0);
    metric[0] = 0;

    std::size_t observed_index = 0;
    std::size_t puncture_index = 0;
    std::size_t next_puncture = puncture_count == 0
                                    ? full_count
                                    : ((puncture_index + 1) * full_count) / (puncture_count + 1);

    for (std::size_t pos = 0; pos < total_steps; ++pos) {
        uint8_t observed = 0;
        uint8_t mask = 0;
        for (int lane = 0; lane < 2; ++lane) {
            std::size_t full_pos = pos * 2 + lane;
            uint8_t bit_mask = static_cast<uint8_t>(lane == 0 ? 2 : 1);
            if (full_pos == next_puncture) {
                ++puncture_index;
                next_puncture = puncture_index < puncture_count
                                    ? ((puncture_index + 1) * full_count) / (puncture_count + 1)
                                    : full_count;
            } else {
                std::size_t physical = interleaver.physical_index(observed_index);
                if (get_bit(codeword, physical) != 0) {
                    observed = static_cast<uint8_t>(observed | bit_mask);
                }
                mask = static_cast<uint8_t>(mask | bit_mask);
                ++observed_index;
            }
        }

        for (int state = 0; state < state_count; ++state) {
            int idx = state * 2;
            int prev0 = state >> 1;
            int prev1 = prev0 | top_prev_bit;
            int candidate0 = metric[prev0] + DIST[trellis.out[idx] & mask][observed];
            int candidate1 = metric[prev1] + DIST[trellis.out[idx + 1] & mask][observed];

            if (candidate0 <= candidate1) {
                next_metric[state] = static_cast<uint16_t>(candidate0);
                previous_choice[pos * state_count + state] = 0;
            } else {
                next_metric[state] = static_cast<uint16_t>(candidate1);
                previous_choice[pos * state_count + state] = 1;
            }
        }

        metric.swap(next_metric);
    }

    std::vector<uint8_t> decoded(total_steps, 0);
    int state = 0;
    for (std::size_t rev = total_steps; rev > 0; --rev) {
        std::size_t pos = rev - 1;
        decoded[pos] = static_cast<uint8_t>(state & 1);
        uint8_t choice = previous_choice[pos * state_count + state];
        state = (state >> 1) | (choice ? top_prev_bit : 0);
    }

    decoded.resize(message_bits);
    return decoded;
}

bool use_large_code(std::size_t message_size) {
    return message_size > LARGE_CODE_THRESHOLD_BYTES;
}

}  // namespace

std::vector<uint8_t> ecc_encode(const std::vector<uint8_t>& message, int t) {
    const std::size_t message_bits = message.size() * 8;
    const std::size_t code_bytes = target_code_bytes(message.size(), t);
    const std::size_t target_bits = code_bytes * 8;
    (void)message_bits;
    return use_large_code(message.size())
               ? convolutional_encode_bytes<LARGE_K, LARGE_G0, LARGE_G1>(message, target_bits, t)
               : convolutional_encode_bytes<SMALL_K, SMALL_G0, SMALL_G1>(message, target_bits, t);
}

std::vector<uint8_t> ecc_decode(const std::vector<uint8_t>& codeword, std::size_t message_size, int t) {
    const std::size_t code_bytes = std::min(codeword.size(), message_size * 2);
    const std::size_t encoded_bits_count = code_bytes * 8;
    const std::size_t message_bits = message_size * 8;
    std::vector<uint8_t> decoded_bits = use_large_code(message_size)
                                            ? viterbi_decode_bytes<LARGE_K, LARGE_G0, LARGE_G1>(
                                                  codeword, encoded_bits_count, message_bits, t)
                                            : viterbi_decode_bytes<SMALL_K, SMALL_G0, SMALL_G1>(
                                                  codeword, encoded_bits_count, message_bits, t);
    return bits_to_bytes(decoded_bits, message_size);
}
