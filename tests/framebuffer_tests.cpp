#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>

#include "cadenza/core/mono_framebuffer.h"

TEST_CASE("supported framebuffer profiles have exact dimensions and storage") {
  cadenza::MonoFramebuffer tembed{cadenza::FramebufferProfile::TEmbed};
  CHECK(tembed.valid());
  CHECK(tembed.width() == 320);
  CHECK(tembed.height() == 170);
  CHECK(tembed.stride() == 40);
  CHECK(tembed.sizeBytes() == 6800);

  cadenza::MonoFramebuffer sharp{cadenza::FramebufferProfile::Sharp};
  CHECK(sharp.valid());
  CHECK(sharp.width() == 400);
  CHECK(sharp.height() == 240);
  CHECK(sharp.stride() == 50);
  CHECK(sharp.sizeBytes() == 12000);
  CHECK(sharp.capacityBytes() == 12000);
}

TEST_CASE("pixels use row-major MSB-first addressing with one meaning black") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::Sharp};
  REQUIRE(framebuffer.setPixel(0, 0, true));
  REQUIRE(framebuffer.setPixel(7, 0, true));
  REQUIRE(framebuffer.setPixel(8, 0, true));
  REQUIRE(framebuffer.setPixel(0, 1, true));

  CHECK(framebuffer.data()[0] == 0x81);
  CHECK(framebuffer.data()[1] == 0x80);
  CHECK(framebuffer.data()[framebuffer.stride()] == 0x80);
  CHECK(framebuffer.pixel(0, 0));
  CHECK_FALSE(framebuffer.pixel(1, 0));

  REQUIRE(framebuffer.setPixel(0, 0, false));
  CHECK(framebuffer.data()[0] == 0x01);
  CHECK_FALSE(framebuffer.pixel(0, 0));
}

TEST_CASE("clear touches active storage only") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  std::fill(framebuffer.data() + framebuffer.sizeBytes(),
            framebuffer.data() + framebuffer.capacityBytes(), 0xA5);

  framebuffer.clear(true);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0xFF; }));
  CHECK(std::all_of(framebuffer.data() + framebuffer.sizeBytes(),
                    framebuffer.data() + framebuffer.capacityBytes(),
                    [](std::uint8_t byte) { return byte == 0xA5; }));

  framebuffer.clear(false);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0x00; }));
}

TEST_CASE("out-of-bounds pixel access is rejected without writes") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  framebuffer.clear(false);
  std::fill(framebuffer.data() + framebuffer.sizeBytes(),
            framebuffer.data() + framebuffer.capacityBytes(), 0x5A);

  CHECK_FALSE(framebuffer.setPixel(-1, 0, true));
  CHECK_FALSE(framebuffer.setPixel(0, -1, true));
  CHECK_FALSE(framebuffer.setPixel(framebuffer.width(), 0, true));
  CHECK_FALSE(framebuffer.setPixel(0, framebuffer.height(), true));
  CHECK_FALSE(framebuffer.pixel(-1, 0));
  CHECK_FALSE(framebuffer.pixel(framebuffer.width(), framebuffer.height()));

  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0x00; }));
  CHECK(std::all_of(framebuffer.data() + framebuffer.sizeBytes(),
                    framebuffer.data() + framebuffer.capacityBytes(),
                    [](std::uint8_t byte) { return byte == 0x5A; }));
}
