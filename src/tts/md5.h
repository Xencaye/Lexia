#pragma once
// MD5 (RFC 1321), only to check a downloaded Voice against the digest the piper-voices catalogue
// gives. It guards against a damaged or cut-off download, not against an attacker: MD5 is
// broken for that, and the catalogue offers nothing stronger. Win32-free.
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

class Md5 {
public:
    Md5();
    void Update(const void* data, size_t size);
    // The digest as 32 lower-case hex digits. Call once; the object is spent after that.
    std::string Hex();

private:
    void Block(const unsigned char* p);
    uint32_t state_[4];
    uint64_t bytes_ = 0;
    unsigned char buffer_[64];
};

// The digest of a file's bytes; empty if the file cannot be read.
std::string Md5OfFile(const std::filesystem::path& file);
