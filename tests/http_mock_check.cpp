#include "SimHttpFetch.h"
#include <cassert>
#include <filesystem>
#include <fstream>

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::filesystem::path root(argv[1]);
  auto write = [&](const char *path, const char *bytes) {
    const auto file = root / path;
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << bytes;
  };
  write("snapshots/rev/source/firmware/LICENSE", "firmware");
  write("snapshots/rev/source/hypher/LICENSE", "hypher");
  write("snapshots/rev/source/empty", "");
  write("LICENSE", "legacy");
  write("fonts/nested/font.bin", "font");
  std::ofstream(root.parent_path() / "outside") << "outside";
  std::filesystem::create_symlink(root.parent_path() / "outside",
                                  root / "snapshots/rev/source/escape");
  setenv("CROSSINK_SIM_HTTP_MOCK_ROOT", argv[1], 1);
  auto check = [&](const char *url, int status, const char *body) {
    sim_http_fetch::Response out;
    assert(sim_http_fetch::fetchFromMockRoot(url, out));
    assert(out.statusCode == status && out.body == body);
  };
  check("https://example.test/snapshots/rev/source/firmware/LICENSE", 200,
        "firmware");
  check("https://example.test/snapshots/rev/source/hypher/LICENSE?token=x", 200,
        "hypher");
  check("https://example.test/snapshots/rev/source/missing/LICENSE", 404, "");
  check("https://example.test/snapshots/rev/source/empty", 200, "");
  check("https://example.test/snapshots/rev/source/escape", 404, "");
  check("https://example.test/snapshots/../LICENSE", 404, "");
  check("https://example.test/snapshots/%2e%2e/LICENSE", 404, "");
  check("https://example.test/snapshots/%00/LICENSE", 404, "");
  check("https://example.test/snapshots//LICENSE", 404, "");
  check("https://example.test/snapshots/rev%5c/LICENSE", 404, "");
  check("https://fonts.test/download/font.bin", 200, "font");
  check("https://books.test/LICENSE", 200, "legacy");
  std::puts(
      "PASS: 12 full-path/duplicate-name/missing/empty/containment/legacy HTTP "
      "mock checks");
}
