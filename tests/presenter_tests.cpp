#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>

#include "cadenza/core/presenter.h"

namespace {
class RecordingPresenter final : public cadenza::FramebufferPresenter {
 public:
  bool present(const cadenza::MonoFramebuffer& framebuffer) noexcept override {
    width = framebuffer.width();
    height = framebuffer.height();
    size = framebuffer.sizeBytes();
    std::copy_n(framebuffer.data(), size, bytes.data());
    ++calls;
    return true;
  }

  std::array<std::uint8_t, cadenza::MonoFramebuffer::kCapacityBytes> bytes{};
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::size_t size = 0;
  int calls = 0;
};
}  // namespace

TEST_CASE("presenter observes canonical framebuffer without mutating it") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  framebuffer.setPixel(0, 0);
  framebuffer.setPixel(319, 169);
  const auto before = framebuffer;
  RecordingPresenter presenter;

  REQUIRE(presenter.present(framebuffer));
  CHECK(presenter.calls == 1);
  CHECK(presenter.width == 320);
  CHECK(presenter.height == 170);
  CHECK(presenter.size == 6800);
  CHECK(presenter.bytes[0] == 0x80);
  CHECK(std::equal(framebuffer.data(),
                   framebuffer.data() + framebuffer.sizeBytes(),
                   before.data()));
}
