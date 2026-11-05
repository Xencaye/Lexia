// Unit tests for tts/md5.h, against the test suite of RFC 1321 (appendix A.5).
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "tts/md5.h"

namespace {

std::string Md5Of(const std::string& s) {
    Md5 m;
    m.Update(s.data(), s.size());
    return m.Hex();
}

}  // namespace

TEST_CASE("Md5: the RFC 1321 test suite") {
    CHECK(Md5Of("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(Md5Of("a") == "0cc175b9c0f1b6a831c399e269772661");
    CHECK(Md5Of("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(Md5Of("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(Md5Of("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b");
    CHECK(Md5Of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") == "d174ab98d277d9f5a5611c2c9f419d9f");
    CHECK(Md5Of("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
          "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_CASE("Md5: the answer does not depend on how the bytes are split into Updates") {
    const std::string text(1000, 'x');  // longer than one 64-byte block, not a multiple of it
    Md5 whole;
    whole.Update(text.data(), text.size());
    Md5 pieces;
    for (size_t i = 0; i < text.size(); i += 7) pieces.Update(text.data() + i, std::min<size_t>(7, text.size() - i));
    CHECK(pieces.Hex() == whole.Hex());
}

TEST_CASE("Md5OfFile: the digest of a file, empty for a file that is not there") {
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "lexia_md5_test.bin";
    {
        std::ofstream out(file, std::ios::binary);
        out << "abc";
    }
    CHECK(Md5OfFile(file) == "900150983cd24fb0d6963f7d28e17f72");
    std::filesystem::remove(file);
    CHECK(Md5OfFile(file).empty());
}
