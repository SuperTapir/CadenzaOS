#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "cadenza/core/transition.h"
#include "cadenza/host/headless_host.h"

namespace {
using Bytes = std::array<std::uint8_t, cadenza::MonoFramebuffer::kCapacityBytes>;

Bytes bytes(const cadenza::MonoFramebuffer& framebuffer) {
  Bytes copy{};
  std::copy_n(framebuffer.data(), framebuffer.sizeBytes(), copy.data());
  return copy;
}

bool equals(const cadenza::MonoFramebuffer& framebuffer, const Bytes& expected) {
  return std::equal(framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes(),
                    expected.data());
}

struct TransitionCase {
  const char* name;
  const cadenza::Transition* transition;
  std::uint64_t tEmbedMidpoint;
  std::uint64_t sharpMidpoint;
};
}  // namespace

TEST_CASE("required transitions have exact endpoints immutable inputs and golden midpoints") {
  const std::array<TransitionCase, 6> transitions{{
      {"dip", &cadenza::kDipTransition, 574572978806829760ULL,
       8083643154240457962ULL},
      {"horizontal", &cadenza::kHorizontalWipeTransition,
       17993305429244896444ULL, 12799766234325660698ULL},
      {"diagonal", &cadenza::kDiagonalWipeTransition,
       12473429395033893090ULL, 17307597515433166450ULL},
      {"iris", &cadenza::kIrisTransition, 10086128000617184134ULL,
       5113660893895041993ULL},
      {"blinds", &cadenza::kVenetianBlindsTransition,
       574572978806829760ULL, 8083643154240457962ULL},
      {"dissolve", &cadenza::kCheckerDissolveTransition,
       2118673648592188952ULL, 9017061332886338522ULL},
  }};
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    cadenza::MonoFramebuffer outgoing{profile};
    cadenza::MonoFramebuffer incoming{profile};
    cadenza::MonoFramebuffer output{profile};
    cadenza::MonoCanvas outgoingCanvas{outgoing};
    cadenza::MonoCanvas incomingCanvas{incoming};
    cadenza::MonoCanvas outputCanvas{output};
    outgoingCanvas.fillDither({0, 0, outgoing.width(), outgoing.height()},
                              cadenza::kOrderedDither8x8, 16, 0, 0);
    incomingCanvas.fillDither({0, 0, incoming.width(), incoming.height()},
                              cadenza::kOrderedDither8x8, 48, 1, 1);
    const Bytes outgoingBytes = bytes(outgoing);
    const Bytes incomingBytes = bytes(incoming);

    for (const auto& transition : transitions) {
      CAPTURE(transition.name);
      CAPTURE(static_cast<int>(profile));
      transition.transition->compose(outgoing, incoming, outputCanvas, 0.0F);
      CHECK(equals(output, outgoingBytes));
      transition.transition->compose(outgoing, incoming, outputCanvas, 1.0F);
      CHECK(equals(output, incomingBytes));
      transition.transition->compose(outgoing, incoming, outputCanvas, 0.5F);
      const std::uint64_t expected =
          profile == cadenza::FramebufferProfile::TEmbed
              ? transition.tEmbedMidpoint
              : transition.sharpMidpoint;
      CHECK(cadenza::host::framebufferHash(output) == expected);
      CHECK(equals(outgoing, outgoingBytes));
      CHECK(equals(incoming, incomingBytes));
    }
  }
}

TEST_CASE("venetian blinds blade count is configurable without changing endpoints") {
  cadenza::VenetianBlindsTransition four{4};
  cadenza::VenetianBlindsTransition twelve{12};
  CHECK(four.bladeCount() == 4);
  CHECK(twelve.bladeCount() == 12);
  cadenza::MonoFramebuffer outgoing{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer incoming{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer fourOutput{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer twelveOutput{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas fourCanvas{fourOutput};
  cadenza::MonoCanvas twelveCanvas{twelveOutput};
  incoming.clear(true);
  four.compose(outgoing, incoming, fourCanvas, 0.25F);
  twelve.compose(outgoing, incoming, twelveCanvas, 0.25F);
  CHECK(cadenza::host::framebufferHash(fourOutput) !=
        cadenza::host::framebufferHash(twelveOutput));
  four.compose(outgoing, incoming, fourCanvas, 1.0F);
  CHECK(cadenza::host::framebufferHash(fourOutput) ==
        cadenza::host::framebufferHash(incoming));
}
