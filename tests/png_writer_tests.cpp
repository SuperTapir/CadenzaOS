#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <png.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "cadenza/desktop/png_writer.h"

TEST_CASE("screenshot filename contains profile frame and simulation metadata") {
  CHECK(cadenza::desktop::screenshotFilename(320, 170, 42, 1234) ==
        "cadenza-320x170-f00000042-t000001234ms.png");
}

TEST_CASE("PNG screenshot decodes to exact dimensions and monochrome colors") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  framebuffer.setPixel(0, 0, true);
  framebuffer.setPixel(319, 169, true);
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "cadenza-png-roundtrip.png";
  REQUIRE(cadenza::desktop::writePng(path.string(), framebuffer));

  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  REQUIRE(png_image_begin_read_from_file(&image, path.c_str()) != 0);
  CHECK(image.width == 320);
  CHECK(image.height == 170);
  image.format = PNG_FORMAT_GRAY;
  std::vector<png_byte> decoded(PNG_IMAGE_SIZE(image));
  REQUIRE(png_image_finish_read(&image, nullptr, decoded.data(), 0, nullptr) != 0);
  CHECK(decoded[0] == 0);
  CHECK(decoded[1] == 255);
  CHECK(decoded[decoded.size() - 1] == 0);
  png_image_free(&image);
  CHECK(std::remove(path.c_str()) == 0);
}
