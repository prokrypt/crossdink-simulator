#include <mbedtls/sha256.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
  assert(argc == 3);
  const size_t chunk = std::strtoul(argv[2], nullptr, 10);
  assert(chunk > 0 && chunk <= 4096);
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  assert(mbedtls_sha256_starts(nullptr, 0) != 0);
  assert(mbedtls_sha256_starts(&ctx, 0) == 0);
  assert(mbedtls_sha256_update(
             &ctx, reinterpret_cast<const unsigned char *>("discard"), 7) == 0);
  // Restart a partially accumulated digest before hashing the supplied vector.
  assert(mbedtls_sha256_starts(&ctx, 0) == 0);
  assert(mbedtls_sha256_update(&ctx, nullptr, 0) == 0);
  assert(mbedtls_sha256_update(&ctx, nullptr, 1) != 0);
  std::ifstream input(argv[1], std::ios::binary);
  assert(input.is_open());
  std::vector<unsigned char> bytes(chunk);
  while (input) {
    input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
    assert(mbedtls_sha256_update(&ctx, bytes.data(),
                                 static_cast<size_t>(input.gcount())) == 0);
  }
  assert(input.eof());
  std::array<unsigned char, 32> output{};
  assert(mbedtls_sha256_finish(&ctx, nullptr) != 0);
  assert(mbedtls_sha256_finish(&ctx, output.data()) == 0);
  for (const auto byte : output)
    std::printf("%02x", byte);
  std::puts("");
  mbedtls_sha256_free(&ctx);
  mbedtls_sha256_init(&ctx);
  assert(mbedtls_sha256_starts(&ctx, 0) == 0);
  assert(mbedtls_sha256_finish(&ctx, output.data()) == 0);
  constexpr unsigned char emptyPrefix[] = {0xe3, 0xb0, 0xc4, 0x42};
  for (size_t i = 0; i < sizeof(emptyPrefix); ++i)
    assert(output[i] == emptyPrefix[i]);
  mbedtls_sha256_free(&ctx);
  mbedtls_sha256_free(nullptr);
}
