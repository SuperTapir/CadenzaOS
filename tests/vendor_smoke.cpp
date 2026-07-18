#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>

extern "C" {
#include "u8g2.h"
}

namespace {
constexpr int kWidth = 400;
constexpr int kHeight = 240;
constexpr int kStride = kWidth / 8;

const u8x8_display_info_t kDisplayInfo = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    kStride, kHeight / 8, 0, 0, kWidth, kHeight,
};
}  // namespace

TEST_CASE("curated U8g2 draws into Cadenza's canonical framebuffer") {
  std::array<std::uint8_t, kStride * kHeight> framebuffer{};
  u8g2_t context{};
  context.u8x8.display_info = &kDisplayInfo;
  u8g2_SetupBuffer(&context, framebuffer.data(), kHeight / 8,
                   u8g2_ll_hvline_horizontal_right_lsb, U8G2_R0);

  u8g2_DrawPixel(&context, 0, 0);
  u8g2_DrawPixel(&context, 7, 0);
  u8g2_DrawPixel(&context, 8, 0);

  CHECK(framebuffer[0] == 0x81);
  CHECK(framebuffer[1] == 0x80);
  CHECK(framebuffer[kStride] == 0x00);
}

TEST_CASE("curated U8g2 font is linked and rasterized without a device") {
  std::array<std::uint8_t, kStride * kHeight> framebuffer{};
  u8g2_t context{};
  context.u8x8.display_info = &kDisplayInfo;
  u8g2_SetupBuffer(&context, framebuffer.data(), kHeight / 8,
                   u8g2_ll_hvline_horizontal_right_lsb, U8G2_R0);
  u8g2_SetFont(&context, u8g2_font_6x10_tf);

  CHECK(u8g2_DrawStr(&context, 0, 10, "Cadenza") > 0);
  CHECK(u8g2_GetStrWidth(&context, "Cadenza") > 0);
}
