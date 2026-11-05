#include "tts/md5.h"

#include <cstring>
#include <fstream>
#include <vector>

// MD5 (RFC 1321), only to check that a downloaded Voice is the file the catalogue lists (Hugging Face
// gives an MD5 for each file). It is not used for security. The data goes in 64-byte blocks; a
// partial block waits in buffer_ until more data fills it, and Hex() pads the last one.
namespace {

// Rotate left: the bits that leave on the left come back on the right.
uint32_t Rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

// K[i] = floor(2^32 * abs(sin(i + 1))), from RFC 1321.
const uint32_t kK[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
// The rotation of each of the 64 steps, also from RFC 1321 (four rounds of 16 steps).
const int kShift[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                        5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

}  // namespace

Md5::Md5() : state_{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}, buffer_{} {}  // RFC 1321 start values

// Mixes one 64-byte block into the state: four rounds of 16 steps, each round with its own function
// f of b, c, d and its own order g of the block's 16 words.
void Md5::Block(const unsigned char* p) {
    uint32_t m[16];
    for (int i = 0; i < 16; ++i) {  // little-endian words, whatever the machine
        m[i] = static_cast<uint32_t>(p[i * 4]) | static_cast<uint32_t>(p[i * 4 + 1]) << 8 |
               static_cast<uint32_t>(p[i * 4 + 2]) << 16 | static_cast<uint32_t>(p[i * 4 + 3]) << 24;
    }
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    for (int i = 0; i < 64; ++i) {
        uint32_t f;
        int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
        else { f = c ^ (b | ~d); g = (7 * i) % 16; }
        const uint32_t next = b + Rotl(a + f + kK[i] + m[g], kShift[i]);
        a = d;
        d = c;
        c = b;
        b = next;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

// Adds data of any size. Whole blocks are mixed in at once; the rest is kept for the next call.
void Md5::Update(const void* data, size_t size) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    size_t fill = static_cast<size_t>(bytes_ % 64);  // bytes already waiting in buffer_
    bytes_ += size;
    if (fill > 0) {  // first top up the waiting partial block
        const size_t take = size < 64 - fill ? size : 64 - fill;
        std::memcpy(buffer_ + fill, p, take);
        p += take;
        size -= take;
        if (fill + take < 64) return;
        Block(buffer_);
    }
    for (; size >= 64; p += 64, size -= 64) Block(p);
    if (size > 0) std::memcpy(buffer_, p, size);
}

// Ends the hash and returns it as 32 lower-case hex digits. The padding is the RFC's: one 0x80 byte,
// zeros up to 8 bytes before a block's end, then the data's length in bits (little-endian). Call it
// once: it adds the padding to the state.
std::string Md5::Hex() {
    const uint64_t bits = bytes_ * 8;
    const unsigned char pad = 0x80;
    Update(&pad, 1);
    const unsigned char zero = 0;
    while (bytes_ % 64 != 56) Update(&zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = static_cast<unsigned char>(bits >> (8 * i));
    Update(len, 8);
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    for (uint32_t word : state_) {
        for (int i = 0; i < 4; ++i) {
            const unsigned char byte = static_cast<unsigned char>(word >> (8 * i));
            out += kHex[byte >> 4];
            out += kHex[byte & 15];
        }
    }
    return out;
}

// The MD5 of a whole file, read 64 KB at a time (a Voice model is about 60 MB). "" if it cannot be read.
std::string Md5OfFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    Md5 md5;
    std::vector<char> buf(1 << 16);
    while (in.read(buf.data(), static_cast<std::streamsize>(buf.size())) || in.gcount() > 0) {
        md5.Update(buf.data(), static_cast<size_t>(in.gcount()));
    }
    if (in.bad()) return {};
    return md5.Hex();
}
